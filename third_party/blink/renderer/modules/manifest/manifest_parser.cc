// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "net/base/mime_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/modules/navigatorcontentutils/navigator_content_utils.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace blink {

namespace {

static constexpr char kOriginWildcardPrefix[] = "%2A.";
// Keep in sync with web_app_origin_association_task.cc.
static wtf_size_t kMaxUrlHandlersSize = 10;
static wtf_size_t kMaxScopeExtensionsSize = 10;
static wtf_size_t kMaxShortcutsSize = 10;
static wtf_size_t kMaxOriginLength = 2000;

// The max number of file extensions an app can handle via the File Handling
// API.
const int kFileHandlerExtensionLimit = 300;
int g_file_handler_extension_limit_for_testing = 0;

bool IsValidMimeType(const String& mime_type) {
  if (mime_type.StartsWith('.')) {
    return true;
  }
  return net::ParseMimeTypeWithoutParameter(mime_type.Utf8(), nullptr, nullptr);
}

bool VerifyFiles(const Vector<mojom::blink::ManifestFileFilterPtr>& files) {
  for (const auto& file : files) {
    for (const auto& accept_type : file->accept) {
      if (!IsValidMimeType(accept_type.LowerASCII())) {
        return false;
      }
    }
  }
  return true;
}

// Determines whether |url| is within scope of |scope|.
bool URLIsWithinScope(const KURL& url, const KURL& scope) {
  return SecurityOrigin::AreSameOrigin(url, scope) &&
         url.GetPath().ToString().StartsWith(scope.GetPath());
}

bool IsHostValidForScopeExtension(String host) {
  if (url::HostIsIPAddress(host.Utf8())) {
    return true;
  }

  const size_t registry_length =
      net::registry_controlled_domains::PermissiveGetHostRegistryLength(
          host.Utf8(),
          // Reject unknown registries (registries that don't have any matches
          // in effective TLD names).
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          // Skip matching private registries that allow external users to
          // specify sub-domains, e.g. glitch.me, as this is allowed.
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

  // Host cannot be a TLD or invalid.
  if (registry_length == 0 || registry_length == std::string::npos ||
      registry_length >= host.length()) {
    return false;
  }

  return true;
}

static bool IsCrLfOrTabChar(UChar c) {
  return c == '\n' || c == '\r' || c == '\t';
}

std::optional<mojom::blink::ManifestFileHandler::LaunchType>
FileHandlerLaunchTypeFromString(const std::string& launch_type) {
  if (WTF::EqualIgnoringASCIICase(String(launch_type), "single-client")) {
    return mojom::blink::ManifestFileHandler::LaunchType::kSingleClient;
  }
  if (WTF::EqualIgnoringASCIICase(String(launch_type), "multiple-clients")) {
    return mojom::blink::ManifestFileHandler::LaunchType::kMultipleClients;
  }
  return std::nullopt;
}

bool IsDefaultManifest(const mojom::blink::Manifest& manifest,
                       const KURL& document_url) {
  if (manifest.has_custom_id || manifest.has_valid_specified_start_url) {
    return false;
  }
  auto default_manifest = mojom::blink::Manifest::New();
  default_manifest->start_url = document_url;
  KURL default_id = document_url;
  default_id.RemoveFragmentIdentifier();
  default_manifest->id = default_id;
  default_manifest->scope = KURL(document_url.BaseAsString().ToString());
  return manifest == *default_manifest;
}

static const char kUMAIdParseResult[] = "Manifest.ParseIdResult";

// Record that the Manifest was successfully parsed. If it is a default
// Manifest, it will recorded as so and nothing will happen. Otherwise, the
// presence of each properties will be recorded.
void ParseSucceeded(const mojom::blink::ManifestPtr& manifest,
                    const KURL& document_url) {
  if (IsDefaultManifest(*manifest, document_url)) {
    return;
  }

  base::UmaHistogramBoolean("Manifest.HasProperty.name",
                            !manifest->name.empty());
  base::UmaHistogramBoolean("Manifest.HasProperty.short_name",
                            !manifest->short_name.empty());
  base::UmaHistogramBoolean("Manifest.HasProperty.description",
                            !manifest->description.empty());
  base::UmaHistogramBoolean("Manifest.HasProperty.start_url",
                            !manifest->start_url.IsEmpty());
  base::UmaHistogramBoolean(
      "Manifest.HasProperty.display",
      manifest->display != blink::mojom::DisplayMode::kUndefined);
  base::UmaHistogramBoolean(
      "Manifest.HasProperty.orientation",
      manifest->orientation !=
          device::mojom::blink::ScreenOrientationLockType::DEFAULT);
  base::UmaHistogramBoolean("Manifest.HasProperty.icons",
                            !manifest->icons.empty());
  base::UmaHistogramBoolean("Manifest.HasProperty.screenshots",
                            !manifest->screenshots.empty());
  base::UmaHistogramBoolean("Manifest.HasProperty.share_target",
                            manifest->share_target.get());
  base::UmaHistogramBoolean("Manifest.HasProperty.protocol_handlers",
                            !manifest->protocol_handlers.empty());
  base::UmaHistogramBoolean("Manifest.HasProperty.gcm_sender_id",
                            !manifest->gcm_sender_id.empty());
}

// Returns a liburlpattern::Part list obtained from running
// liburlpattern::Parse on a UrlPatternInit field. The list will be empty if the
// field is empty. Returns std::nullopt if the field should be rejected or the
// parse failed, e.g. if it contains custom (or ill-formed) regex.
std::optional<std::vector<liburlpattern::Part>> ParsePatternInitField(
    const std::optional<String>& field,
    const String default_field_value) {
  const String value = field.has_value() ? field.value() : default_field_value;
  if (value.empty()) {
    return std::vector<liburlpattern::Part>();
  }

  StringUTF8Adaptor utf8(value);
  auto parse_result = liburlpattern::Parse(
      utf8.AsStringView(),
      [](std::string_view input) { return std::string(input); });

  if (parse_result.ok()) {
    std::vector<liburlpattern::Part> part_list;
    for (auto& part : parse_result.value().PartList()) {
      // We don't allow custom regex for security reasons as this will be
      // used in the browser process.
      if (part.type == liburlpattern::PartType::kRegex) {
        return std::nullopt;
      }

      part_list.push_back(std::move(part));
    }
    return part_list;
  }
  return std::nullopt;
}

String EscapePatternString(const StringView& input) {
  std::string result;
  result.reserve(input.length());
  StringUTF8Adaptor utf8(input);
  liburlpattern::EscapePatternStringAndAppend(utf8.AsStringView(), result);
  return String(result);
}

// Utility function to determine if a pathname is absolute or not. We do some
// additional checking for escaped or grouped slashes.
//
// Note: This is partially copied from
// third_party/blink/renderer/core/url_pattern/url_pattern.cc
bool IsAbsolutePathname(String pathname) {
  if (pathname.empty()) {
    return false;
  }

  if (pathname[0] == '/') {
    return true;
  }

  if (pathname.length() < 2) {
    return false;
  }

  // Patterns treat escaped slashes and slashes within an explicit grouping as
  // valid leading slashes.  For example, "\/foo" or "{/foo}".  Patterns do
  // not consider slashes within a custom regexp group as valid for the leading
  // pathname slash for now.  To support that we would need to be able to
  // detect things like ":name_123(/foo)" as a valid leading group in a pattern,
  // but that is considered too complex for now.
  if ((pathname[0] == '\\' || pathname[0] == '{') && pathname[1] == '/') {
    return true;
  }

  return false;
}

String ResolveRelativePathnamePattern(const KURL& base_url, String pathname) {
  if (base_url.IsStandard() && !IsAbsolutePathname(pathname)) {
    String base_path = EscapePatternString(base_url.GetPath());
    auto slash_index = base_path.ReverseFind('/');
    if (slash_index != WTF::kNotFound) {
      // Extract the base_url path up to and including the last slash. Append
      // the relative pathname to it.
      base_path.Truncate(slash_index + 1);
      base_path = base_path + pathname;
      return base_path;
    }
  }
  return pathname;
}

}  // anonymous namespace

ManifestParser::ManifestParser(const String& data,
                               const KURL& manifest_url,
                               const KURL& document_url,
                               ExecutionContext* execution_context)
    : data_(data),
      manifest_url_(manifest_url),
      document_url_(document_url),
      execution_context_(execution_context),
      failed_(false) {}

ManifestParser::~ManifestParser() {}

// static
void ManifestParser::SetFileHandlerExtensionLimitForTesting(int limit) {
  g_file_handler_extension_limit_for_testing = limit;
}

bool ManifestParser::Parse() {
  DCHECK(!manifest_);

  // TODO(crbug.com/1264024): Deprecate JSON comments here, if possible.
  JSONParseError error;

  bool has_comments = false;
  std::unique_ptr<JSONValue> root =
      ParseJSONWithCommentsDeprecated(data_, &error, &has_comments);
  manifest_ = mojom::blink::Manifest::New();
  if (!root) {
    AddErrorInfo(error.message, true, error.line, error.column);
    failed_ = true;
    return false;
  }

  std::unique_ptr<JSONObject> root_object = JSONObject::From(std::move(root));
  if (!root_object) {
    AddErrorInfo("root element must be a valid JSON object.", true);
    failed_ = true;
    return false;
  }

  manifest_->manifest_url = manifest_url_;
  manifest_->dir = ParseDir(root_object.get());
  manifest_->name = ParseName(root_object.get());
  manifest_->short_name = ParseShortName(root_object.get());
  manifest_->description = ParseDescription(root_object.get());
  const auto& [start_url, start_url_parse_result] =
      ParseStartURL(root_object.get(), document_url_);
  manifest_->start_url = start_url;
  manifest_->has_valid_specified_start_url =
      start_url_parse_result == ParseStartUrlResult::kParsedFromJson;

  const auto& [id, id_parse_result] =
      ParseId(root_object.get(), manifest_->start_url);
  manifest_->id = id;
  manifest_->has_custom_id = id_parse_result == ParseIdResultType::kSucceed;

  manifest_->scope = ParseScope(root_object.get(), manifest_->start_url);
  manifest_->display = ParseDisplay(root_object.get());
  manifest_->display_override = ParseDisplayOverride(root_object.get());
  manifest_->orientation = ParseOrientation(root_object.get());
  manifest_->icons = ParseIcons(root_object.get());
  manifest_->screenshots = ParseScreenshots(root_object.get());

  auto share_target = ParseShareTarget(root_object.get());
  if (share_target.has_value()) {
    manifest_->share_target = std::move(*share_target);
  }

  manifest_->file_handlers = ParseFileHandlers(root_object.get());
  manifest_->protocol_handlers = ParseProtocolHandlers(root_object.get());
  manifest_->url_handlers = ParseUrlHandlers(root_object.get());
  manifest_->scope_extensions = ParseScopeExtensions(root_object.get());
  manifest_->lock_screen = ParseLockScreen(root_object.get());
  manifest_->note_taking = ParseNoteTaking(root_object.get());
  manifest_->related_applications = ParseRelatedApplications(root_object.get());
  manifest_->prefer_related_applications =
      ParsePreferRelatedApplications(root_object.get());

  std::optional<RGBA32> theme_color = ParseThemeColor(root_object.get());
  manifest_->has_theme_color = theme_color.has_value();
  if (manifest_->has_theme_color) {
    manifest_->theme_color = *theme_color;
  }

  std::optional<RGBA32> background_color =
      ParseBackgroundColor(root_object.get());
  manifest_->has_background_color = background_color.has_value();
  if (manifest_->has_background_color) {
    manifest_->background_color = *background_color;
  }

  manifest_->gcm_sender_id = ParseGCMSenderID(root_object.get());
  manifest_->shortcuts = ParseShortcuts(root_object.get());

  manifest_->permissions_policy =
      ParseIsolatedAppPermissions(root_object.get());

  manifest_->launch_handler = ParseLaunchHandler(root_object.get());

  if (RuntimeEnabledFeatures::WebAppTranslationsEnabled(execution_context_)) {
    manifest_->translations = ParseTranslations(root_object.get());
  }

  if (RuntimeEnabledFeatures::WebAppTabStripCustomizationsEnabled(
          execution_context_)) {
    manifest_->tab_strip = ParseTabStrip(root_object.get());
  }

  manifest_->version = ParseVersion(root_object.get());

  ParseSucceeded(manifest_, document_url_);
  base::UmaHistogramEnumeration(kUMAIdParseResult, id_parse_result);

  return has_comments;
}

mojom::blink::ManifestPtr ManifestParser::TakeManifest() {
  return std::move(manifest_);
}

void ManifestParser::TakeErrors(
    Vector<mojom::blink::ManifestErrorPtr>* errors) {
  errors->clear();
  errors->swap(errors_);
}

bool ManifestParser::failed() const {
  return failed_;
}

ManifestParser::PatternInit::PatternInit(std::optional<String> protocol,
                                         std::optional<String> username,
                                         std::optional<String> password,
                                         std::optional<String> hostname,
                                         std::optional<String> port,
                                         std::optional<String> pathname,
                                         std::optional<String> search,
                                         std::optional<String> hash,
                                         KURL base_url)
    : protocol(std::move(protocol)),
      username(std::move(username)),
      password(std::move(password)),
      hostname(std::move(hostname)),
      port(std::move(port)),
      pathname(std::move(pathname)),
      search(std::move(search)),
      hash(std::move(hash)),
      base_url(base_url) {}
ManifestParser::PatternInit::~PatternInit() = default;
ManifestParser::PatternInit::PatternInit(PatternInit&&) = default;
ManifestParser::PatternInit& ManifestParser::PatternInit::operator=(
    PatternInit&&) = default;

bool ManifestParser::PatternInit::IsAbsolute() const {
  return protocol.has_value() || hostname.has_value() || port.has_value();
}

bool ManifestParser::ParseBoolean(const JSONObject* object,
                                  const String& key,
                                  bool default_value) {
  JSONValue* json_value = object->Get(key);
  if (!json_value) {
    return default_value;
  }

  bool value;
  if (!json_value->AsBoolean(&value)) {
    AddErrorInfo("property '" + key + "' ignored, type " + "boolean expected.");
    return default_value;
  }

  return value;
}

std::optional<String> ManifestParser::ParseString(const JSONObject* object,
                                                  const String& key,
                                                  Trim trim) {
  JSONValue* json_value = object->Get(key);
  if (!json_value) {
    return std::nullopt;
  }

  String value;
  if (!json_value->AsString(&value) || value.IsNull()) {
    AddErrorInfo("property '" + key + "' ignored, type " + "string expected.");
    return std::nullopt;
  }

  if (trim) {
    value = value.StripWhiteSpace();
  }
  return value;
}

std::optional<String> ManifestParser::ParseStringForMember(
    const JSONObject* object,
    const String& member_name,
    const String& key,
    bool required,
    Trim trim) {
  JSONValue* json_value = object->Get(key);
  if (!json_value) {
    if (required) {
      AddErrorInfo("property '" + key + "' of '" + member_name +
                   "' not present.");
    }

    return std::nullopt;
  }

  String value;
  if (!json_value->AsString(&value)) {
    AddErrorInfo("property '" + key + "' of '" + member_name +
                 "' ignored, type string expected.");
    return std::nullopt;
  }
  if (trim) {
    value = value.StripWhiteSpace();
  }

  if (value == "") {
    AddErrorInfo("property '" + key + "' of '" + member_name +
                 "' is an empty string.");
    if (required) {
      return std::nullopt;
    }
  }

  return value;
}

std::optional<RGBA32> ManifestParser::ParseColor(const JSONObject* object,
                                                 const String& key) {
  std::optional<String> parsed_color = ParseString(object, key, Trim(true));
  if (!parsed_color.has_value()) {
    return std::nullopt;
  }

  Color color;
  if (!CSSParser::ParseColor(color, *parsed_color, true)) {
    AddErrorInfo("property '" + key + "' ignored, '" + *parsed_color +
                 "' is not a " + "valid color.");
    return std::nullopt;
  }

  return color.Rgb();
}

KURL ManifestParser::ParseURL(const JSONObject* object,
                              const String& key,
                              const KURL& base_url,
                              ParseURLRestrictions origin_restriction,
                              bool ignore_empty_string) {
  std::optional<String> url_str = ParseString(object, key, Trim(false));
  if (!url_str.has_value()) {
    return KURL();
  }
  if (ignore_empty_string && url_str.value() == "") {
    return KURL();
  }

  KURL resolved = KURL(base_url, *url_str);
  if (!resolved.IsValid()) {
    AddErrorInfo("property '" + key + "' ignored, URL is invalid.");
    return KURL();
  }

  switch (origin_restriction) {
    case ParseURLRestrictions::kNoRestrictions:
      return resolved;
    case ParseURLRestrictions::kSameOriginOnly:
      if (!SecurityOrigin::AreSameOrigin(resolved, document_url_)) {
        AddErrorInfo("property '" + key +
                     "' ignored, should be same origin as document.");
        return KURL();
      }
      return resolved;
    case ParseURLRestrictions::kWithinScope:
      if (!URLIsWithinScope(resolved, manifest_->scope)) {
        AddErrorInfo("property '" + key +
                     "' ignored, should be within scope of the manifest.");
        return KURL();
      }

      // Within scope implies same origin as document URL.
      DCHECK(SecurityOrigin::AreSameOrigin(resolved, document_url_));

      return resolved;
  }

  NOTREACHED_IN_MIGRATION();
  return KURL();
}

template <typename Enum>
Enum ManifestParser::ParseFirstValidEnum(const JSONObject* object,
                                         const String& key,
                                         Enum (*parse_enum)(const std::string&),
                                         Enum invalid_value) {
  const JSONValue* value = object->Get(key);
  if (!value) {
    return invalid_value;
  }

  String string_value;
  if (value->AsString(&string_value)) {
    Enum enum_value = parse_enum(string_value.Utf8());
    if (enum_value == invalid_value) {
      AddErrorInfo(key + " value '" + string_value +
                   "' ignored, unknown value.");
    }
    return enum_value;
  }

  const JSONArray* list = JSONArray::Cast(value);
  if (!list) {
    AddErrorInfo("property '" + key +
                 "' ignored, type string or array of strings expected.");
    return invalid_value;
  }

  for (wtf_size_t i = 0; i < list->size(); ++i) {
    const JSONValue* item = list->at(i);
    if (!item->AsString(&string_value)) {
      AddErrorInfo(key + " value '" + item->ToJSONString() +
                   "' ignored, string expected.");
      continue;
    }

    Enum enum_value = parse_enum(string_value.Utf8());
    if (enum_value != invalid_value) {
      return enum_value;
    }

    AddErrorInfo(key + " value '" + string_value + "' ignored, unknown value.");
  }

  return invalid_value;
}

mojom::blink::Manifest::TextDirection ManifestParser::ParseDir(
    const JSONObject* object) {
  using TextDirection = mojom::blink::Manifest::TextDirection;

  std::optional<String> dir = ParseString(object, "dir", Trim(true));
  if (!dir.has_value()) {
    return TextDirection::kAuto;
  }

  std::optional<TextDirection> textDirection =
      TextDirectionFromString(dir->Utf8());
  if (!textDirection.has_value()) {
    AddErrorInfo("unknown 'dir' value ignored.");
    return TextDirection::kAuto;
  }

  return *textDirection;
}

String ManifestParser::ParseName(const JSONObject* object) {
  std::optional<String> name = ParseString(object, "name", Trim(true));
  if (name.has_value()) {
    name = name->RemoveCharacters(IsCrLfOrTabChar);
    if (name->length() == 0) {
      name = std::nullopt;
    }
  }
  return name.has_value() ? *name : String();
}

String ManifestParser::ParseShortName(const JSONObject* object) {
  std::optional<String> short_name =
      ParseString(object, "short_name", Trim(true));
  if (short_name.has_value()) {
    short_name = short_name->RemoveCharacters(IsCrLfOrTabChar);
    if (short_name->length() == 0) {
      short_name = std::nullopt;
    }
  }
  return short_name.has_value() ? *short_name : String();
}

String ManifestParser::ParseDescription(const JSONObject* object) {
  std::optional<String> description =
      ParseString(object, "description", Trim(true));
  return description.has_value() ? *description : String();
}

std::pair<KURL, ManifestParser::ParseIdResultType> ManifestParser::ParseId(
    const JSONObject* object,
    const KURL& start_url) {
  if (!start_url.IsValid()) {
    return {KURL(), ParseIdResultType::kInvalidStartUrl};
  }
  KURL start_url_origin = KURL(SecurityOrigin::Create(start_url)->ToString());

  KURL id = ParseURL(object, "id", start_url_origin,
                     ParseURLRestrictions::kSameOriginOnly,
                     /*ignore_empty_string=*/true);
  ParseIdResultType parse_result;
  if (id.IsValid()) {
    parse_result = ParseIdResultType::kSucceed;
  } else {
    // If id is not specified, sets to start_url
    parse_result = ParseIdResultType::kDefaultToStartUrl;
    id = start_url;
  }
  id.RemoveFragmentIdentifier();
  return {id, parse_result};
}

std::pair<KURL, ManifestParser::ParseStartUrlResult>
ManifestParser::ParseStartURL(const JSONObject* object,
                              const KURL& document_url) {
  KURL start_url = ParseURL(object, "start_url", manifest_url_,
                            ParseURLRestrictions::kSameOriginOnly);
  if (start_url.IsEmpty()) {
    return std::make_pair(document_url,
                          ParseStartUrlResult::kDefaultDocumentUrl);
  }
  return std::make_pair(start_url, ParseStartUrlResult::kParsedFromJson);
}

KURL ManifestParser::ParseScope(const JSONObject* object,
                                const KURL& start_url) {
  KURL scope = ParseURL(object, "scope", manifest_url_,
                        ParseURLRestrictions::kNoRestrictions);
  const KURL& default_value = start_url;
  DCHECK(default_value.IsValid());

  if (scope.IsEmpty()) {
    return KURL(default_value.BaseAsString().ToString());
  }

  if (!URLIsWithinScope(default_value, scope)) {
    AddErrorInfo(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.");
    return KURL(default_value.BaseAsString().ToString());
  }

  scope.RemoveFragmentIdentifier();
  scope.SetQuery(String());

  DCHECK(scope.IsValid());
  DCHECK(SecurityOrigin::AreSameOrigin(scope, document_url_));
  return scope;
}

blink::mojom::DisplayMode ManifestParser::ParseDisplay(
    const JSONObject* object) {
  std::optional<String> display = ParseString(object, "display", Trim(true));
  if (!display.has_value()) {
    return blink::mojom::DisplayMode::kUndefined;
  }

  blink::mojom::DisplayMode display_enum =
      DisplayModeFromString(display->Utf8());

  if (display_enum == mojom::blink::DisplayMode::kUndefined) {
    AddErrorInfo("unknown 'display' value ignored.");
    return display_enum;
  }

  // Ignore "enhanced" display modes.
  if (!IsBasicDisplayMode(display_enum)) {
    display_enum = mojom::blink::DisplayMode::kUndefined;
    AddErrorInfo("inapplicable 'display' value ignored.");
  }

  return display_enum;
}

Vector<mojom::blink::DisplayMode> ManifestParser::ParseDisplayOverride(
    const JSONObject* object) {
  Vector<mojom::blink::DisplayMode> display_override;

  JSONValue* json_value = object->Get("display_override");
  if (!json_value) {
    return display_override;
  }

  JSONArray* display_override_list = object->GetArray("display_override");
  if (!display_override_list) {
    AddErrorInfo("property 'display_override' ignored, type array expected.");
    return display_override;
  }

  for (wtf_size_t i = 0; i < display_override_list->size(); ++i) {
    String display_enum_string;
    // AsString will return an empty string if a type error occurs,
    // which will cause DisplayModeFromString to return kUndefined,
    // resulting in this entry being ignored.
    display_override_list->at(i)->AsString(&display_enum_string);
    display_enum_string = display_enum_string.StripWhiteSpace();
    mojom::blink::DisplayMode display_enum =
        DisplayModeFromString(display_enum_string.Utf8());

    if (!RuntimeEnabledFeatures::WebAppTabStripEnabled(execution_context_) &&
        display_enum == mojom::blink::DisplayMode::kTabbed) {
      display_enum = mojom::blink::DisplayMode::kUndefined;
    }

    if (!base::FeatureList::IsEnabled(blink::features::kWebAppBorderless) &&
        display_enum == mojom::blink::DisplayMode::kBorderless) {
      display_enum = mojom::blink::DisplayMode::kUndefined;
    }

    if (display_enum != mojom::blink::DisplayMode::kUndefined) {
      display_override.push_back(display_enum);
    }
  }

  return display_override;
}

device::mojom::blink::ScreenOrientationLockType
ManifestParser::ParseOrientation(const JSONObject* object) {
  std::optional<String> orientation =
      ParseString(object, "orientation", Trim(true));

  if (!orientation.has_value()) {
    return device::mojom::blink::ScreenOrientationLockType::DEFAULT;
  }

  device::mojom::blink::ScreenOrientationLockType orientation_enum =
      WebScreenOrientationLockTypeFromString(orientation->Utf8());
  if (orientation_enum ==
      device::mojom::blink::ScreenOrientationLockType::DEFAULT) {
    AddErrorInfo("unknown 'orientation' value ignored.");
  }
  return orientation_enum;
}

KURL ManifestParser::ParseIconSrc(const JSONObject* icon) {
  return ParseURL(icon, "src", manifest_url_,
                  ParseURLRestrictions::kNoRestrictions);
}

String ManifestParser::ParseIconType(const JSONObject* icon) {
  std::optional<String> type = ParseString(icon, "type", Trim(true));
  return type.has_value() ? *type : String("");
}

Vector<gfx::Size> ManifestParser::ParseIconSizes(const JSONObject* icon) {
  std::optional<String> sizes_str = ParseString(icon, "sizes", Trim(false));
  if (!sizes_str.has_value()) {
    return Vector<gfx::Size>();
  }

  WebVector<gfx::Size> web_sizes =
      WebIconSizesParser::ParseIconSizes(WebString(*sizes_str));
  Vector<gfx::Size> sizes;
  for (auto& size : web_sizes) {
    sizes.push_back(size);
  }

  if (sizes.empty()) {
    AddErrorInfo("found icon with no valid size.");
  }
  return sizes;
}

std::optional<Vector<mojom::blink::ManifestImageResource::Purpose>>
ManifestParser::ParseIconPurpose(const JSONObject* icon) {
  std::optional<String> purpose_str = ParseString(icon, "purpose", Trim(false));
  Vector<mojom::blink::ManifestImageResource::Purpose> purposes;

  if (!purpose_str.has_value()) {
    purposes.push_back(mojom::blink::ManifestImageResource::Purpose::ANY);
    return purposes;
  }

  Vector<String> keywords;
  purpose_str.value().Split(/*separator=*/" ", /*allow_empty_entries=*/false,
                            keywords);

  // "any" is the default if there are no other keywords.
  if (keywords.empty()) {
    purposes.push_back(mojom::blink::ManifestImageResource::Purpose::ANY);
    return purposes;
  }

  bool unrecognised_purpose = false;
  for (auto& keyword : keywords) {
    keyword = keyword.StripWhiteSpace();
    if (keyword.empty()) {
      continue;
    }

    if (EqualIgnoringASCIICase(keyword, "any")) {
      purposes.push_back(mojom::blink::ManifestImageResource::Purpose::ANY);
    } else if (EqualIgnoringASCIICase(keyword, "monochrome")) {
      purposes.push_back(
          mojom::blink::ManifestImageResource::Purpose::MONOCHROME);
    } else if (EqualIgnoringASCIICase(keyword, "maskable")) {
      purposes.push_back(
          mojom::blink::ManifestImageResource::Purpose::MASKABLE);
    } else {
      unrecognised_purpose = true;
    }
  }

  // This implies there was at least one purpose given, but none recognised.
  // Instead of defaulting to "any" (which would not be future proof),
  // invalidate the whole icon.
  if (purposes.empty()) {
    AddErrorInfo("found icon with no valid purpose; ignoring it.");
    return std::nullopt;
  }

  if (unrecognised_purpose) {
    AddErrorInfo(
        "found icon with one or more invalid purposes; those purposes are "
        "ignored.");
  }

  return purposes;
}

mojom::blink::ManifestScreenshot::FormFactor
ManifestParser::ParseScreenshotFormFactor(const JSONObject* screenshot) {
  std::optional<String> form_factor_str =
      ParseString(screenshot, "form_factor", Trim(false));

  if (!form_factor_str.has_value()) {
    return mojom::blink::ManifestScreenshot::FormFactor::kUnknown;
  }

  String form_factor = form_factor_str.value();

  if (EqualIgnoringASCIICase(form_factor, "wide")) {
    return mojom::blink::ManifestScreenshot::FormFactor::kWide;
  } else if (EqualIgnoringASCIICase(form_factor, "narrow")) {
    return mojom::blink::ManifestScreenshot::FormFactor::kNarrow;
  }

  AddErrorInfo(
      "property 'form_factor' on screenshots has an invalid value, ignoring "
      "it.");

  return mojom::blink::ManifestScreenshot::FormFactor::kUnknown;
}

String ManifestParser::ParseScreenshotLabel(const JSONObject* object) {
  std::optional<String> label = ParseString(object, "label", Trim(true));
  return label.has_value() ? *label : String();
}

Vector<mojom::blink::ManifestImageResourcePtr> ManifestParser::ParseIcons(
    const JSONObject* object) {
  return ParseImageResourceArray("icons", object);
}

Vector<mojom::blink::ManifestScreenshotPtr> ManifestParser::ParseScreenshots(
    const JSONObject* object) {
  Vector<mojom::blink::ManifestScreenshotPtr> screenshots;
  JSONValue* json_value = object->Get("screenshots");
  if (!json_value) {
    return screenshots;
  }

  JSONArray* screenshots_list = object->GetArray("screenshots");
  if (!screenshots_list) {
    AddErrorInfo("property 'screenshots' ignored, type array expected.");
    return screenshots;
  }

  for (wtf_size_t i = 0; i < screenshots_list->size(); ++i) {
    JSONObject* screenshot_object = JSONObject::Cast(screenshots_list->at(i));
    if (!screenshot_object) {
      continue;
    }

    auto screenshot = mojom::blink::ManifestScreenshot::New();
    auto image = ParseImageResource(screenshot_object);
    if (!image.has_value()) {
      continue;
    }

    screenshot->image = std::move(*image);
    screenshot->form_factor = ParseScreenshotFormFactor(screenshot_object);
    screenshot->label = ParseScreenshotLabel(screenshot_object);

    screenshots.push_back(std::move(screenshot));
  }

  return screenshots;
}

Vector<mojom::blink::ManifestImageResourcePtr>
ManifestParser::ParseImageResourceArray(const String& key,
                                        const JSONObject* object) {
  Vector<mojom::blink::ManifestImageResourcePtr> icons;
  JSONValue* json_value = object->Get(key);
  if (!json_value) {
    return icons;
  }

  JSONArray* icons_list = object->GetArray(key);
  if (!icons_list) {
    AddErrorInfo("property '" + key + "' ignored, type array expected.");
    return icons;
  }

  for (wtf_size_t i = 0; i < icons_list->size(); ++i) {
    auto icon = ParseImageResource(icons_list->at(i));
    if (icon.has_value()) {
      icons.push_back(std::move(*icon));
    }
  }

  return icons;
}

std::optional<mojom::blink::ManifestImageResourcePtr>
ManifestParser::ParseImageResource(const JSONValue* object) {
  const JSONObject* icon_object = JSONObject::Cast(object);
  if (!icon_object) {
    return std::nullopt;
  }

  auto icon = mojom::blink::ManifestImageResource::New();
  icon->src = ParseIconSrc(icon_object);
  // An icon MUST have a valid src. If it does not, it MUST be ignored.
  if (!icon->src.IsValid()) {
    return std::nullopt;
  }

  icon->type = ParseIconType(icon_object);
  icon->sizes = ParseIconSizes(icon_object);
  auto purpose = ParseIconPurpose(icon_object);
  if (!purpose) {
    return std::nullopt;
  }

  icon->purpose = std::move(*purpose);
  return icon;
}

String ManifestParser::ParseShortcutName(const JSONObject* shortcut) {
  std::optional<String> name =
      ParseStringForMember(shortcut, "shortcut", "name", true, Trim(true));
  return name.has_value() ? *name : String();
}

String ManifestParser::ParseShortcutShortName(const JSONObject* shortcut) {
  std::optional<String> short_name = ParseStringForMember(
      shortcut, "shortcut", "short_name", false, Trim(true));
  return short_name.has_value() ? *short_name : String();
}

String ManifestParser::ParseShortcutDescription(const JSONObject* shortcut) {
  std::optional<String> description = ParseStringForMember(
      shortcut, "shortcut", "description", false, Trim(true));
  return description.has_value() ? *description : String();
}

KURL ManifestParser::ParseShortcutUrl(const JSONObject* shortcut) {
  KURL shortcut_url = ParseURL(shortcut, "url", manifest_url_,
                               ParseURLRestrictions::kWithinScope);
  if (shortcut_url.IsNull()) {
    AddErrorInfo("property 'url' of 'shortcut' not present.");
  }

  return shortcut_url;
}

Vector<mojom::blink::ManifestShortcutItemPtr> ManifestParser::ParseShortcuts(
    const JSONObject* object) {
  Vector<mojom::blink::ManifestShortcutItemPtr> shortcuts;
  JSONValue* json_value = object->Get("shortcuts");
  if (!json_value) {
    return shortcuts;
  }

  JSONArray* shortcuts_list = object->GetArray("shortcuts");
  if (!shortcuts_list) {
    AddErrorInfo("property 'shortcuts' ignored, type array expected.");
    return shortcuts;
  }

  for (wtf_size_t i = 0; i < shortcuts_list->size(); ++i) {
    if (i == kMaxShortcutsSize) {
      AddErrorInfo("property 'shortcuts' contains more than " +
                   String::Number(kMaxShortcutsSize) +
                   " valid elements, only the first " +
                   String::Number(kMaxShortcutsSize) + " are parsed.");
      break;
    }

    JSONObject* shortcut_object = JSONObject::Cast(shortcuts_list->at(i));
    if (!shortcut_object) {
      continue;
    }

    auto shortcut = mojom::blink::ManifestShortcutItem::New();
    shortcut->url = ParseShortcutUrl(shortcut_object);
    // A shortcut MUST have a valid url. If it does not, it MUST be ignored.
    if (!shortcut->url.IsValid()) {
      continue;
    }

    // A shortcut MUST have a valid name. If it does not, it MUST be ignored.
    shortcut->name = ParseShortcutName(shortcut_object);
    if (shortcut->name == String()) {
      continue;
    }

    shortcut->short_name = ParseShortcutShortName(shortcut_object);
    shortcut->description = ParseShortcutDescription(shortcut_object);
    auto icons = ParseIcons(shortcut_object);
    if (!icons.empty()) {
      shortcut->icons = std::move(icons);
    }

    shortcuts.push_back(std::move(shortcut));
  }

  return shortcuts;
}

String ManifestParser::ParseFileFilterName(const JSONObject* file) {
  if (!file->Get("name")) {
    AddErrorInfo("property 'name' missing.");
    return String("");
  }

  String value;
  if (!file->GetString("name", &value)) {
    AddErrorInfo("property 'name' ignored, type string expected.");
    return String("");
  }
  return value;
}

Vector<String> ManifestParser::ParseFileFilterAccept(const JSONObject* object) {
  Vector<String> accept_types;
  if (!object->Get("accept")) {
    return accept_types;
  }

  String accept_str;
  if (object->GetString("accept", &accept_str)) {
    accept_types.push_back(accept_str);
    return accept_types;
  }

  JSONArray* accept_list = object->GetArray("accept");
  if (!accept_list) {
    // 'accept' property is the wrong type. Returning an empty vector here
    // causes the 'files' entry to be discarded.
    AddErrorInfo("property 'accept' ignored, type array or string expected.");
    return accept_types;
  }

  for (wtf_size_t i = 0; i < accept_list->size(); ++i) {
    JSONValue* accept_value = accept_list->at(i);
    String accept_string;
    if (!accept_value || !accept_value->AsString(&accept_string)) {
      // A particular 'accept' entry is invalid - just drop that one entry.
      AddErrorInfo("'accept' entry ignored, expected to be of type string.");
      continue;
    }
    accept_types.push_back(accept_string);
  }

  return accept_types;
}

Vector<mojom::blink::ManifestFileFilterPtr> ManifestParser::ParseTargetFiles(
    const String& key,
    const JSONObject* from) {
  Vector<mojom::blink::ManifestFileFilterPtr> files;
  if (!from->Get(key)) {
    return files;
  }

  JSONArray* file_list = from->GetArray(key);
  if (!file_list) {
    // https://wicg.github.io/web-share-target/level-2/#share_target-member
    // step 5 indicates that the 'files' attribute is allowed to be a single
    // (non-array) FileFilter.
    const JSONObject* file_object = from->GetJSONObject(key);
    if (!file_object) {
      AddErrorInfo(
          "property 'files' ignored, type array or FileFilter expected.");
      return files;
    }

    ParseFileFilter(file_object, &files);
    return files;
  }
  for (wtf_size_t i = 0; i < file_list->size(); ++i) {
    const JSONObject* file_object = JSONObject::Cast(file_list->at(i));
    if (!file_object) {
      AddErrorInfo("files must be a sequence of non-empty file entries.");
      continue;
    }

    ParseFileFilter(file_object, &files);
  }

  return files;
}

void ManifestParser::ParseFileFilter(
    const JSONObject* file_object,
    Vector<mojom::blink::ManifestFileFilterPtr>* files) {
  auto file = mojom::blink::ManifestFileFilter::New();
  file->name = ParseFileFilterName(file_object);
  if (file->name.empty()) {
    // https://wicg.github.io/web-share-target/level-2/#share_target-member
    // step 7.1 requires that we invalidate this FileFilter if 'name' is an
    // empty string. We also invalidate if 'name' is undefined or not a
    // string.
    return;
  }

  file->accept = ParseFileFilterAccept(file_object);
  if (file->accept.empty()) {
    return;
  }

  files->push_back(std::move(file));
}

std::optional<mojom::blink::ManifestShareTarget::Method>
ManifestParser::ParseShareTargetMethod(const JSONObject* share_target_object) {
  if (!share_target_object->Get("method")) {
    AddErrorInfo(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.");
    return mojom::blink::ManifestShareTarget::Method::kGet;
  }

  String value;
  if (!share_target_object->GetString("method", &value)) {
    return std::nullopt;
  }

  String method = value.UpperASCII();
  if (method == "GET") {
    return mojom::blink::ManifestShareTarget::Method::kGet;
  }
  if (method == "POST") {
    return mojom::blink::ManifestShareTarget::Method::kPost;
  }

  return std::nullopt;
}

std::optional<mojom::blink::ManifestShareTarget::Enctype>
ManifestParser::ParseShareTargetEnctype(const JSONObject* share_target_object) {
  if (!share_target_object->Get("enctype")) {
    AddErrorInfo(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded");
    return mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded;
  }

  String value;
  if (!share_target_object->GetString("enctype", &value)) {
    return std::nullopt;
  }

  String enctype = value.LowerASCII();
  if (enctype == "application/x-www-form-urlencoded") {
    return mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded;
  }

  if (enctype == "multipart/form-data") {
    return mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData;
  }

  return std::nullopt;
}

mojom::blink::ManifestShareTargetParamsPtr
ManifestParser::ParseShareTargetParams(const JSONObject* share_target_params) {
  auto params = mojom::blink::ManifestShareTargetParams::New();

  // NOTE: These are key names for query parameters, which are filled with share
  // data. As such, |params.url| is just a string.
  std::optional<String> text =
      ParseString(share_target_params, "text", Trim(true));
  params->text = text.has_value() ? *text : String();
  std::optional<String> title =
      ParseString(share_target_params, "title", Trim(true));
  params->title = title.has_value() ? *title : String();
  std::optional<String> url =
      ParseString(share_target_params, "url", Trim(true));
  params->url = url.has_value() ? *url : String();

  auto files = ParseTargetFiles("files", share_target_params);
  if (!files.empty()) {
    params->files = std::move(files);
  }
  return params;
}

std::optional<mojom::blink::ManifestShareTargetPtr>
ManifestParser::ParseShareTarget(const JSONObject* object) {
  const JSONObject* share_target_object = object->GetJSONObject("share_target");
  if (!share_target_object) {
    return std::nullopt;
  }

  auto share_target = mojom::blink::ManifestShareTarget::New();
  share_target->action = ParseURL(share_target_object, "action", manifest_url_,
                                  ParseURLRestrictions::kWithinScope);
  if (!share_target->action.IsValid()) {
    AddErrorInfo(
        "property 'share_target' ignored. Property 'action' is "
        "invalid.");
    return std::nullopt;
  }

  auto method = ParseShareTargetMethod(share_target_object);
  auto enctype = ParseShareTargetEnctype(share_target_object);

  const JSONObject* share_target_params_object =
      share_target_object->GetJSONObject("params");
  if (!share_target_params_object) {
    AddErrorInfo(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.");
    return std::nullopt;
  }

  share_target->params = ParseShareTargetParams(share_target_params_object);
  if (!method.has_value()) {
    AddErrorInfo(
        "invalid method. Allowed methods are:"
        "GET and POST.");
    return std::nullopt;
  }
  share_target->method = method.value();

  if (!enctype.has_value()) {
    AddErrorInfo(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.");
    return std::nullopt;
  }
  share_target->enctype = enctype.value();

  if (share_target->method == mojom::blink::ManifestShareTarget::Method::kGet) {
    if (share_target->enctype ==
        mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData) {
      AddErrorInfo(
          "invalid enctype for GET method. Only "
          "application/x-www-form-urlencoded is allowed.");
      return std::nullopt;
    }
  }

  if (share_target->params->files.has_value()) {
    if (share_target->method !=
            mojom::blink::ManifestShareTarget::Method::kPost ||
        share_target->enctype !=
            mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData) {
      AddErrorInfo("files are only supported with multipart/form-data POST.");
      return std::nullopt;
    }
  }

  if (share_target->params->files.has_value() &&
      !VerifyFiles(*share_target->params->files)) {
    AddErrorInfo("invalid mime type inside files.");
    return std::nullopt;
  }

  return share_target;
}

Vector<mojom::blink::ManifestFileHandlerPtr> ManifestParser::ParseFileHandlers(
    const JSONObject* object) {
  if (!object->Get("file_handlers")) {
    return {};
  }

  JSONArray* entry_array = object->GetArray("file_handlers");
  if (!entry_array) {
    AddErrorInfo("property 'file_handlers' ignored, type array expected.");
    return {};
  }

  Vector<mojom::blink::ManifestFileHandlerPtr> result;
  for (wtf_size_t i = 0; i < entry_array->size(); ++i) {
    JSONObject* json_entry = JSONObject::Cast(entry_array->at(i));
    if (!json_entry) {
      AddErrorInfo("FileHandler ignored, type object expected.");
      continue;
    }

    std::optional<mojom::blink::ManifestFileHandlerPtr> entry =
        ParseFileHandler(json_entry);
    if (!entry) {
      continue;
    }

    result.push_back(std::move(entry.value()));
  }

  return result;
}

std::optional<mojom::blink::ManifestFileHandlerPtr>
ManifestParser::ParseFileHandler(const JSONObject* file_handler) {
  mojom::blink::ManifestFileHandlerPtr entry =
      mojom::blink::ManifestFileHandler::New();
  entry->action = ParseURL(file_handler, "action", manifest_url_,
                           ParseURLRestrictions::kWithinScope);
  if (!entry->action.IsValid()) {
    AddErrorInfo("FileHandler ignored. Property 'action' is invalid.");
    return std::nullopt;
  }

  entry->name = ParseString(file_handler, "name", Trim(true)).value_or("");
  const bool feature_enabled =
      base::FeatureList::IsEnabled(blink::features::kFileHandlingIcons) ||
      RuntimeEnabledFeatures::FileHandlingIconsEnabled(execution_context_);
  if (feature_enabled) {
    entry->icons = ParseIcons(file_handler);
  }

  entry->accept = ParseFileHandlerAccept(file_handler->GetJSONObject("accept"));
  if (entry->accept.empty()) {
    AddErrorInfo("FileHandler ignored. Property 'accept' is invalid.");
    return std::nullopt;
  }

  entry->launch_type =
      ParseFirstValidEnum<
          std::optional<mojom::blink::ManifestFileHandler::LaunchType>>(
          file_handler, "launch_type", &FileHandlerLaunchTypeFromString,
          /*invalid_value=*/std::nullopt)
          .value_or(
              mojom::blink::ManifestFileHandler::LaunchType::kSingleClient);

  return entry;
}

HashMap<String, Vector<String>> ManifestParser::ParseFileHandlerAccept(
    const JSONObject* accept) {
  HashMap<String, Vector<String>> result;
  if (!accept) {
    return result;
  }

  const int kExtensionLimit = g_file_handler_extension_limit_for_testing > 0
                                  ? g_file_handler_extension_limit_for_testing
                                  : kFileHandlerExtensionLimit;
  if (total_file_handler_extension_count_ > kExtensionLimit) {
    return result;
  }

  for (wtf_size_t i = 0; i < accept->size(); ++i) {
    JSONObject::Entry entry = accept->at(i);

    // Validate the MIME type.
    String& mimetype = entry.first;
    std::string top_level_mime_type;
    if (!net::ParseMimeTypeWithoutParameter(mimetype.Utf8(),
                                            &top_level_mime_type, nullptr) ||
        !net::IsValidTopLevelMimeType(top_level_mime_type)) {
      AddErrorInfo("invalid MIME type: " + mimetype);
      continue;
    }

    Vector<String> extensions;
    String extension;
    JSONArray* extensions_array = JSONArray::Cast(entry.second);
    if (extensions_array) {
      for (wtf_size_t j = 0; j < extensions_array->size(); ++j) {
        JSONValue* value = extensions_array->at(j);
        if (!value->AsString(&extension)) {
          AddErrorInfo(
              "property 'accept' file extension ignored, type string "
              "expected.");
          continue;
        }

        if (!ParseFileHandlerAcceptExtension(value, &extension)) {
          // Errors are added by ParseFileHandlerAcceptExtension.
          continue;
        }

        extensions.push_back(extension);
      }
    } else if (ParseFileHandlerAcceptExtension(entry.second, &extension)) {
      extensions.push_back(extension);
    } else {
      // Parsing errors will already have been added.
      continue;
    }

    total_file_handler_extension_count_ += extensions.size();
    int extension_overflow =
        total_file_handler_extension_count_ - kExtensionLimit;
    if (extension_overflow > 0) {
      auto erase_iter = extensions.end() - extension_overflow;
      AddErrorInfo(
          "property 'accept': too many total file extensions, ignoring "
          "extensions starting from \"" +
          *erase_iter + "\"");
      extensions.erase(erase_iter, extensions.end());
    }

    if (!extensions.empty()) {
      result.Set(mimetype, std::move(extensions));
    }

    if (extension_overflow > 0) {
      break;
    }
  }

  return result;
}

bool ManifestParser::ParseFileHandlerAcceptExtension(const JSONValue* extension,
                                                     String* output) {
  if (!extension->AsString(output)) {
    AddErrorInfo(
        "property 'accept' type ignored. File extensions must be type array or "
        "type string.");
    return false;
  }

  if (!output->StartsWith(".")) {
    AddErrorInfo(
        "property 'accept' file extension ignored, must start with a '.'.");
    return false;
  }

  return true;
}

Vector<mojom::blink::ManifestProtocolHandlerPtr>
ManifestParser::ParseProtocolHandlers(const JSONObject* from) {
  Vector<mojom::blink::ManifestProtocolHandlerPtr> protocols;

  if (!from->Get("protocol_handlers")) {
    return protocols;
  }

  JSONArray* protocol_list = from->GetArray("protocol_handlers");
  if (!protocol_list) {
    AddErrorInfo("property 'protocol_handlers' ignored, type array expected.");
    return protocols;
  }

  for (wtf_size_t i = 0; i < protocol_list->size(); ++i) {
    const JSONObject* protocol_object = JSONObject::Cast(protocol_list->at(i));
    if (!protocol_object) {
      AddErrorInfo("protocol_handlers entry ignored, type object expected.");
      continue;
    }

    std::optional<mojom::blink::ManifestProtocolHandlerPtr> protocol =
        ParseProtocolHandler(protocol_object);
    if (!protocol) {
      continue;
    }

    protocols.push_back(std::move(protocol.value()));
  }

  return protocols;
}

std::optional<mojom::blink::ManifestProtocolHandlerPtr>
ManifestParser::ParseProtocolHandler(const JSONObject* object) {
  if (!object->Get("protocol")) {
    AddErrorInfo(
        "protocol_handlers entry ignored, required property 'protocol' is "
        "missing.");
    return std::nullopt;
  }

  auto protocol_handler = mojom::blink::ManifestProtocolHandler::New();
  std::optional<String> protocol = ParseString(object, "protocol", Trim(true));
  String error_message;
  bool is_valid_protocol = protocol.has_value();

  if (is_valid_protocol &&
      !VerifyCustomHandlerScheme(protocol.value(), error_message,
                                 ProtocolHandlerSecurityLevel::kStrict)) {
    AddErrorInfo(error_message);
    is_valid_protocol = false;
  }

  if (!is_valid_protocol) {
    AddErrorInfo(
        "protocol_handlers entry ignored, required property 'protocol' is "
        "invalid.");
    return std::nullopt;
  }
  protocol_handler->protocol = protocol.value();

  if (!object->Get("url")) {
    AddErrorInfo(
        "protocol_handlers entry ignored, required property 'url' is missing.");
    return std::nullopt;
  }
  protocol_handler->url = ParseURL(object, "url", manifest_url_,
                                   ParseURLRestrictions::kWithinScope);
  bool is_valid_url = protocol_handler->url.IsValid();
  if (is_valid_url) {
    const char kToken[] = "%s";
    String user_url = protocol_handler->url.GetString();
    String tokenless_url = protocol_handler->url.GetString();
    tokenless_url.Remove(user_url.Find(kToken), std::size(kToken) - 1);
    KURL full_url(manifest_url_, tokenless_url);

    if (!VerifyCustomHandlerURLSyntax(full_url, manifest_url_, user_url,
                                      error_message)) {
      AddErrorInfo(error_message);
      is_valid_url = false;
    }
  }

  if (!is_valid_url) {
    AddErrorInfo(
        "protocol_handlers entry ignored, required property 'url' is invalid.");
    return std::nullopt;
  }

  return std::move(protocol_handler);
}

Vector<mojom::blink::ManifestUrlHandlerPtr> ManifestParser::ParseUrlHandlers(
    const JSONObject* from) {
  Vector<mojom::blink::ManifestUrlHandlerPtr> url_handlers;
  const bool feature_enabled =
      base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers) ||
      RuntimeEnabledFeatures::WebAppUrlHandlingEnabled(execution_context_);
  if (!feature_enabled || !from->Get("url_handlers")) {
    return url_handlers;
  }
  JSONArray* handlers_list = from->GetArray("url_handlers");
  if (!handlers_list) {
    AddErrorInfo("property 'url_handlers' ignored, type array expected.");
    return url_handlers;
  }
  for (wtf_size_t i = 0; i < handlers_list->size(); ++i) {
    if (i == kMaxUrlHandlersSize) {
      AddErrorInfo("property 'url_handlers' contains more than " +
                   String::Number(kMaxUrlHandlersSize) +
                   " valid elements, only the first " +
                   String::Number(kMaxUrlHandlersSize) + " are parsed.");
      break;
    }

    const JSONObject* handler_object = JSONObject::Cast(handlers_list->at(i));
    if (!handler_object) {
      AddErrorInfo("url_handlers entry ignored, type object expected.");
      continue;
    }

    std::optional<mojom::blink::ManifestUrlHandlerPtr> url_handler =
        ParseUrlHandler(handler_object);
    if (!url_handler) {
      continue;
    }
    url_handlers.push_back(std::move(url_handler.value()));
  }
  return url_handlers;
}

std::optional<mojom::blink::ManifestUrlHandlerPtr>
ManifestParser::ParseUrlHandler(const JSONObject* object) {
  DCHECK(
      base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers) ||
      RuntimeEnabledFeatures::WebAppUrlHandlingEnabled(execution_context_));
  if (!object->Get("origin")) {
    AddErrorInfo(
        "url_handlers entry ignored, required property 'origin' is missing.");
    return std::nullopt;
  }
  const std::optional<String> origin_string =
      ParseString(object, "origin", Trim(true));
  if (!origin_string.has_value()) {
    AddErrorInfo(
        "url_handlers entry ignored, required property 'origin' is invalid.");
    return std::nullopt;
  }

  // TODO(crbug.com/1072058): pre-process for input without scheme.
  // (eg. example.com instead of https://example.com) because we can always
  // assume the use of https for URL handling. Remove this TODO if we decide
  // to require fully specified https scheme in this origin input.

  if (origin_string->length() > kMaxOriginLength) {
    AddErrorInfo(
        "url_handlers entry ignored, 'origin' exceeds maximum character length "
        "of " +
        String::Number(kMaxOriginLength) + " .");
    return std::nullopt;
  }

  auto origin = SecurityOrigin::CreateFromString(*origin_string);
  if (!origin || origin->IsOpaque()) {
    AddErrorInfo(
        "url_handlers entry ignored, required property 'origin' is invalid.");
    return std::nullopt;
  }
  if (origin->Protocol() != url::kHttpsScheme) {
    AddErrorInfo(
        "url_handlers entry ignored, required property 'origin' must use the "
        "https scheme.");
    return std::nullopt;
  }

  String host = origin->Host();
  auto url_handler = mojom::blink::ManifestUrlHandler::New();
  // Check for wildcard *.
  if (host.StartsWith(kOriginWildcardPrefix)) {
    url_handler->has_origin_wildcard = true;
    // Trim the wildcard prefix to get the effective host. Minus one to exclude
    // the length of the null terminator.
    host = host.Substring(sizeof(kOriginWildcardPrefix) - 1);
  } else {
    url_handler->has_origin_wildcard = false;
  }

  bool host_valid = IsHostValidForScopeExtension(host);
  if (!host_valid) {
    AddErrorInfo(
        "url_handlers entry ignored, domain of required property 'origin' is "
        "invalid.");
    return std::nullopt;
  }

  if (url_handler->has_origin_wildcard) {
    origin = SecurityOrigin::CreateFromValidTuple(origin->Protocol(), host,
                                                  origin->Port());
    if (!origin_string.has_value()) {
      AddErrorInfo(
          "url_handlers entry ignored, required property 'origin' is invalid.");
      return std::nullopt;
    }
  }

  url_handler->origin = origin;
  return std::move(url_handler);
}

Vector<mojom::blink::ManifestScopeExtensionPtr>
ManifestParser::ParseScopeExtensions(const JSONObject* from) {
  Vector<mojom::blink::ManifestScopeExtensionPtr> scope_extensions;
  const bool feature_enabled =
      base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableScopeExtensions) ||
      RuntimeEnabledFeatures::WebAppScopeExtensionsEnabled(execution_context_);
  if (!feature_enabled || !from->Get("scope_extensions")) {
    return scope_extensions;
  }

  JSONArray* extensions_list = from->GetArray("scope_extensions");
  if (!extensions_list) {
    AddErrorInfo("property 'scope_extensions' ignored, type array expected.");
    return scope_extensions;
  }

  JSONValue::ValueType expected_entry_type = JSONValue::kTypeNull;
  for (wtf_size_t i = 0; i < extensions_list->size(); ++i) {
    if (i == kMaxScopeExtensionsSize) {
      AddErrorInfo("property 'scope_extensions' contains more than " +
                   String::Number(kMaxScopeExtensionsSize) +
                   " valid elements, only the first " +
                   String::Number(kMaxScopeExtensionsSize) + " are parsed.");
      break;
    }

    const JSONValue* extensions_entry = extensions_list->at(i);
    if (!extensions_entry) {
      AddErrorInfo("scope_extensions entry ignored, entry is null.");
      continue;
    }

    JSONValue::ValueType entry_type = extensions_entry->GetType();
    if (entry_type != JSONValue::kTypeString &&
        entry_type != JSONValue::kTypeObject) {
      AddErrorInfo(
          "scope_extensions entry ignored, type string or object expected.");
      continue;
    }

    // Check whether first scope extension entry in the list is a string or
    // object to make sure that following entries have the same type, ignoring
    // entries that are null or other types.
    if (expected_entry_type != JSONValue::kTypeString &&
        expected_entry_type != JSONValue::kTypeObject) {
      expected_entry_type = entry_type;
    }

    std::optional<mojom::blink::ManifestScopeExtensionPtr> scope_extension =
        std::nullopt;
    if (expected_entry_type == JSONValue::kTypeString) {
      String scope_extension_origin;
      if (!extensions_entry->AsString(&scope_extension_origin)) {
        AddErrorInfo("scope_extensions entry ignored, type string expected.");
        continue;
      }
      scope_extension = ParseScopeExtensionOrigin(scope_extension_origin);
    } else {
      const JSONObject* extension_object = JSONObject::Cast(extensions_entry);
      if (!extension_object) {
        AddErrorInfo("scope_extensions entry ignored, type object expected.");
        continue;
      }
      scope_extension = ParseScopeExtension(extension_object);
    }

    if (!scope_extension) {
      continue;
    }
    scope_extensions.push_back(std::move(scope_extension.value()));
  }
  return scope_extensions;
}

std::optional<mojom::blink::ManifestScopeExtensionPtr>
ManifestParser::ParseScopeExtension(const JSONObject* object) {
  DCHECK(
      base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableScopeExtensions) ||
      RuntimeEnabledFeatures::WebAppScopeExtensionsEnabled(execution_context_));
  if (!object->Get("origin")) {
    AddErrorInfo(
        "scope_extensions entry ignored, required property 'origin' is "
        "missing.");
    return std::nullopt;
  }
  const std::optional<String> origin_string =
      ParseString(object, "origin", Trim(true));
  if (!origin_string.has_value()) {
    return std::nullopt;
  }

  return ParseScopeExtensionOrigin(*origin_string);
}

std::optional<mojom::blink::ManifestScopeExtensionPtr>
ManifestParser::ParseScopeExtensionOrigin(const String& origin_string) {
  DCHECK(
      base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableScopeExtensions) ||
      RuntimeEnabledFeatures::WebAppScopeExtensionsEnabled(execution_context_));

  // TODO(crbug.com/1250011): pre-process for input without scheme.
  // (eg. example.com instead of https://example.com) because we can always
  // assume the use of https for scope extensions. Remove this TODO if we decide
  // to require fully specified https scheme in this origin input.

  if (origin_string.length() > kMaxOriginLength) {
    AddErrorInfo(
        "scope_extensions entry ignored, 'origin' exceeds maximum character "
        "length of " +
        String::Number(kMaxOriginLength) + " .");
    return std::nullopt;
  }

  auto origin = SecurityOrigin::CreateFromString(origin_string);
  if (!origin || origin->IsOpaque()) {
    AddErrorInfo(
        "scope_extensions entry ignored, required property 'origin' is "
        "invalid.");
    return std::nullopt;
  }
  if (origin->Protocol() != url::kHttpsScheme) {
    AddErrorInfo(
        "scope_extensions entry ignored, required property 'origin' must use "
        "the https scheme.");
    return std::nullopt;
  }

  String host = origin->Host();
  auto scope_extension = mojom::blink::ManifestScopeExtension::New();
  // Check for wildcard *.
  if (host.StartsWith(kOriginWildcardPrefix)) {
    scope_extension->has_origin_wildcard = true;
    // Trim the wildcard prefix to get the effective host. Minus one to exclude
    // the length of the null terminator.
    host = host.Substring(sizeof(kOriginWildcardPrefix) - 1);
  } else {
    scope_extension->has_origin_wildcard = false;
  }

  bool host_valid = IsHostValidForScopeExtension(host);
  if (!host_valid) {
    AddErrorInfo(
        "scope_extensions entry ignored, domain of required property 'origin' "
        "is invalid.");
    return std::nullopt;
  }

  if (scope_extension->has_origin_wildcard) {
    origin = SecurityOrigin::CreateFromValidTuple(origin->Protocol(), host,
                                                  origin->Port());
    if (!origin) {
      AddErrorInfo(
          "scope_extensions entry ignored, required property 'origin' is "
          "invalid.");
      return std::nullopt;
    }
  }

  scope_extension->origin = origin;
  return std::move(scope_extension);
}

KURL ManifestParser::ParseLockScreenStartUrl(const JSONObject* lock_screen) {
  if (!lock_screen->Get("start_url")) {
    return KURL();
  }
  KURL start_url = ParseURL(lock_screen, "start_url", manifest_url_,
                            ParseURLRestrictions::kWithinScope);
  if (!start_url.IsValid()) {
    // Error already reported by ParseURL.
    return KURL();
  }

  return start_url;
}

mojom::blink::ManifestLockScreenPtr ManifestParser::ParseLockScreen(
    const JSONObject* manifest) {
  if (!manifest->Get("lock_screen")) {
    return nullptr;
  }

  const JSONObject* lock_screen_object = manifest->GetJSONObject("lock_screen");
  if (!lock_screen_object) {
    AddErrorInfo("property 'lock_screen' ignored, type object expected.");
    return nullptr;
  }
  auto lock_screen = mojom::blink::ManifestLockScreen::New();
  lock_screen->start_url = ParseLockScreenStartUrl(lock_screen_object);

  return lock_screen;
}

KURL ManifestParser::ParseNoteTakingNewNoteUrl(const JSONObject* note_taking) {
  if (!note_taking->Get("new_note_url")) {
    return KURL();
  }
  KURL new_note_url = ParseURL(note_taking, "new_note_url", manifest_url_,
                               ParseURLRestrictions::kWithinScope);
  if (!new_note_url.IsValid()) {
    // Error already reported by ParseURL.
    return KURL();
  }

  return new_note_url;
}

mojom::blink::ManifestNoteTakingPtr ManifestParser::ParseNoteTaking(
    const JSONObject* manifest) {
  if (!manifest->Get("note_taking")) {
    return nullptr;
  }

  const JSONObject* note_taking_object = manifest->GetJSONObject("note_taking");
  if (!note_taking_object) {
    AddErrorInfo("property 'note_taking' ignored, type object expected.");
    return nullptr;
  }
  auto note_taking = mojom::blink::ManifestNoteTaking::New();
  note_taking->new_note_url = ParseNoteTakingNewNoteUrl(note_taking_object);

  return note_taking;
}

String ManifestParser::ParseRelatedApplicationPlatform(
    const JSONObject* application) {
  std::optional<String> platform =
      ParseString(application, "platform", Trim(true));
  return platform.has_value() ? *platform : String();
}

std::optional<KURL> ManifestParser::ParseRelatedApplicationURL(
    const JSONObject* application) {
  return ParseURL(application, "url", manifest_url_,
                  ParseURLRestrictions::kNoRestrictions);
}

String ManifestParser::ParseRelatedApplicationId(
    const JSONObject* application) {
  std::optional<String> id = ParseString(application, "id", Trim(true));
  return id.has_value() ? *id : String();
}

Vector<mojom::blink::ManifestRelatedApplicationPtr>
ManifestParser::ParseRelatedApplications(const JSONObject* object) {
  Vector<mojom::blink::ManifestRelatedApplicationPtr> applications;

  JSONValue* value = object->Get("related_applications");
  if (!value) {
    return applications;
  }

  JSONArray* applications_list = object->GetArray("related_applications");
  if (!applications_list) {
    AddErrorInfo(
        "property 'related_applications' ignored,"
        " type array expected.");
    return applications;
  }

  for (wtf_size_t i = 0; i < applications_list->size(); ++i) {
    const JSONObject* application_object =
        JSONObject::Cast(applications_list->at(i));
    if (!application_object) {
      continue;
    }

    auto application = mojom::blink::ManifestRelatedApplication::New();
    application->platform = ParseRelatedApplicationPlatform(application_object);
    // "If platform is undefined, move onto the next item if any are left."
    if (application->platform.empty()) {
      AddErrorInfo(
          "'platform' is a required field, related application"
          " ignored.");
      continue;
    }

    application->id = ParseRelatedApplicationId(application_object);
    application->url = ParseRelatedApplicationURL(application_object);
    // "If both id and url are undefined, move onto the next item if any are
    // left."
    if ((!application->url.has_value() || !application->url->IsValid()) &&
        application->id.empty()) {
      AddErrorInfo(
          "one of 'url' or 'id' is required, related application"
          " ignored.");
      continue;
    }

    applications.push_back(std::move(application));
  }

  return applications;
}

bool ManifestParser::ParsePreferRelatedApplications(const JSONObject* object) {
  return ParseBoolean(object, "prefer_related_applications", false);
}

std::optional<RGBA32> ManifestParser::ParseThemeColor(
    const JSONObject* object) {
  return ParseColor(object, "theme_color");
}

std::optional<RGBA32> ManifestParser::ParseBackgroundColor(
    const JSONObject* object) {
  return ParseColor(object, "background_color");
}

String ManifestParser::ParseGCMSenderID(const JSONObject* object) {
  std::optional<String> gcm_sender_id =
      ParseString(object, "gcm_sender_id", Trim(true));
  return gcm_sender_id.has_value() ? *gcm_sender_id : String();
}

Vector<blink::ParsedPermissionsPolicyDeclaration>
ManifestParser::ParseIsolatedAppPermissions(const JSONObject* object) {
  PermissionsPolicyParser::Node policy{
      OriginWithPossibleWildcards::NodeType::kHeader};

  JSONValue* json_value = object->Get("permissions_policy");
  if (!json_value) {
    return Vector<blink::ParsedPermissionsPolicyDeclaration>();
  }

  JSONObject* permissions_dict = object->GetJSONObject("permissions_policy");
  if (!permissions_dict) {
    AddErrorInfo(
        "property 'permissions_policy' ignored, type object expected.");
    return Vector<blink::ParsedPermissionsPolicyDeclaration>();
  }

  for (wtf_size_t i = 0; i < permissions_dict->size(); ++i) {
    const JSONObject::Entry& entry = permissions_dict->at(i);
    String feature(entry.first);

    JSONArray* origin_allowlist = JSONArray::Cast(entry.second);
    if (!origin_allowlist) {
      AddErrorInfo("permission '" + feature +
                   "' ignored, invalid allowlist: type array expected.");
      continue;
    }

    Vector<String> allowlist = ParseOriginAllowlist(origin_allowlist, feature);
    if (!allowlist.size()) {
      continue;
    }
    PermissionsPolicyParser::Declaration new_policy;
    new_policy.feature_name = feature;
    for (const auto& origin : allowlist) {
      // PermissionsPolicyParser expects 4 types of origin strings:
      // - "self": wrapped in single quotes (as in a header)
      // - "none": wrapped in single quotes (as in a header)
      // - "*" (asterisk): not wrapped
      // - "<origin>": actual origin names should not be wrapped in single
      //        quotes
      // The "src" origin string type can be ignored here as it's only used in
      // the iframe "allow" attribute.
      //
      // Sidenote: Actual origin names ("<origin>") are parsed using
      // OriginWithPossibleWildcards::Parse() which fails if the origin string
      // contains any non-alphanumeric characters, such as a single quote. For
      // this reason, actual origin names must not be wrapped since the parser
      // will just drop them as being improperly formatted (i.e. they would be
      // the equivalent to some manifest containing an origin wrapped in single
      // quotes, which is invalid).
      String wrapped_origin = origin;
      if (EqualIgnoringASCIICase(origin, "self") ||
          EqualIgnoringASCIICase(origin, "none")) {
        wrapped_origin = "'" + origin + "'";
      }
      new_policy.allowlist.push_back(wrapped_origin);
    }
    policy.declarations.push_back(new_policy);
  }

  PolicyParserMessageBuffer logger(
      "Error with permissions_policy manifest field: ");
  blink::ParsedPermissionsPolicy parsed_policy =
      PermissionsPolicyParser::ParsePolicyFromNode(
          policy, SecurityOrigin::Create(manifest_url_), logger,
          execution_context_);

  Vector<blink::ParsedPermissionsPolicyDeclaration> out;
  for (const auto& decl : parsed_policy) {
    out.push_back(std::move(decl));
  }
  return out;
}

Vector<String> ManifestParser::ParseOriginAllowlist(
    const JSONArray* json_allowlist,
    const String& feature) {
  Vector<String> out;
  for (wtf_size_t i = 0; i < json_allowlist->size(); ++i) {
    JSONValue* json_value = json_allowlist->at(i);
    if (!json_value) {
      AddErrorInfo(
          "permissions_policy entry ignored, required property 'origin' is "
          "invalid.");
      return Vector<String>();
    }

    String origin_string;
    if (!json_value->AsString(&origin_string) || origin_string.IsNull()) {
      AddErrorInfo(
          "permissions_policy entry ignored, required property 'origin' "
          "contains "
          "an invalid element: type string expected.");
      return Vector<String>();
    }

    if (!origin_string.length()) {
      AddErrorInfo(
          "permissions_policy entry ignored, required property 'origin' is "
          "contains an empty string.");
      return Vector<String>();
    }

    if (origin_string.length() > kMaxOriginLength) {
      AddErrorInfo(
          "permissions_policy entry ignored, 'origin' exceeds maximum "
          "character length "
          "of " +
          String::Number(kMaxOriginLength) + " .");
      return Vector<String>();
    }
    out.push_back(origin_string);
  }

  return out;
}

mojom::blink::ManifestLaunchHandlerPtr ManifestParser::ParseLaunchHandler(
    const JSONObject* object) {
  const JSONValue* launch_handler_value = object->Get("launch_handler");
  if (!launch_handler_value) {
    return nullptr;
  }

  const JSONObject* launch_handler_object =
      JSONObject::Cast(launch_handler_value);
  if (!launch_handler_object) {
    AddErrorInfo("launch_handler value ignored, object expected.");
    return nullptr;
  }

  using ClientMode = mojom::blink::ManifestLaunchHandler::ClientMode;
  return mojom::blink::ManifestLaunchHandler::New(
      ParseFirstValidEnum<std::optional<ClientMode>>(
          launch_handler_object, "client_mode", &ClientModeFromString,
          /*invalid_value=*/std::nullopt)
          .value_or(ClientMode::kAuto));
}

HashMap<String, mojom::blink::ManifestTranslationItemPtr>
ManifestParser::ParseTranslations(const JSONObject* object) {
  HashMap<String, mojom::blink::ManifestTranslationItemPtr> result;

  if (!object->Get("translations")) {
    return result;
  }

  JSONObject* translations_map = object->GetJSONObject("translations");
  if (!translations_map) {
    AddErrorInfo("property 'translations' ignored, object expected.");
    return result;
  }

  for (wtf_size_t i = 0; i < translations_map->size(); ++i) {
    JSONObject::Entry entry = translations_map->at(i);
    String locale = entry.first;
    if (locale == "") {
      AddErrorInfo("skipping translation, non-empty locale string expected.");
      continue;
    }
    JSONObject* translation = JSONObject::Cast(entry.second);
    if (!translation) {
      AddErrorInfo("skipping translation, object expected.");
      continue;
    }

    auto translation_item = mojom::blink::ManifestTranslationItem::New();

    std::optional<String> name = ParseStringForMember(
        translation, "translations", "name", false, Trim(true));
    translation_item->name =
        name.has_value() && name->length() != 0 ? *name : String();

    std::optional<String> short_name = ParseStringForMember(
        translation, "translations", "short_name", false, Trim(true));
    translation_item->short_name =
        short_name.has_value() && short_name->length() != 0 ? *short_name
                                                            : String();

    std::optional<String> description = ParseStringForMember(
        translation, "translations", "description", false, Trim(true));
    translation_item->description =
        description.has_value() && description->length() != 0 ? *description
                                                              : String();

    // A translation may be specified for any combination of translatable fields
    // in the manifest. If no translations are supplied, we skip this item.
    if (!translation_item->name && !translation_item->short_name &&
        !translation_item->description) {
      continue;
    }

    result.Set(locale, std::move(translation_item));
  }
  return result;
}

mojom::blink::ManifestTabStripPtr ManifestParser::ParseTabStrip(
    const JSONObject* object) {
  if (!object->Get("tab_strip")) {
    return nullptr;
  }

  JSONObject* tab_strip_object = object->GetJSONObject("tab_strip");
  if (!tab_strip_object) {
    AddErrorInfo("property 'tab_strip' ignored, object expected.");
    return nullptr;
  }

  auto result = mojom::blink::ManifestTabStrip::New();

  JSONValue* home_tab_value = tab_strip_object->Get("home_tab");
  if (home_tab_value && home_tab_value->GetType() == JSONValue::kTypeObject) {
    JSONObject* home_tab_object = tab_strip_object->GetJSONObject("home_tab");
    auto home_tab_params = mojom::blink::HomeTabParams::New();

    JSONValue* home_tab_icons = home_tab_object->Get("icons");
    String string_value;
    if (home_tab_icons && !(home_tab_icons->AsString(&string_value) &&
                            EqualIgnoringASCIICase(string_value, "auto"))) {
      home_tab_params->icons = ParseIcons(home_tab_object);
    }

    home_tab_params->scope_patterns = ParseScopePatterns(home_tab_object);

    result->home_tab =
        mojom::blink::HomeTabUnion::NewParams(std::move(home_tab_params));
  } else {
    result->home_tab = mojom::blink::HomeTabUnion::NewVisibility(
        ParseTabStripMemberVisibility(home_tab_value));
  }

  auto new_tab_button_params = mojom::blink::NewTabButtonParams::New();

  JSONObject* new_tab_button_object =
      tab_strip_object->GetJSONObject("new_tab_button");
  if (new_tab_button_object) {
    JSONValue* new_tab_button_url = new_tab_button_object->Get("url");

    String string_value;
    if (new_tab_button_url && !(new_tab_button_url->AsString(&string_value) &&
                                EqualIgnoringASCIICase(string_value, "auto"))) {
      KURL url = ParseURL(new_tab_button_object, "url", manifest_url_,
                          ParseURLRestrictions::kWithinScope);
      if (!url.IsNull()) {
        new_tab_button_params->url = url;
      }
    }
  }
  result->new_tab_button = std::move(new_tab_button_params);

  return result;
}

mojom::blink::TabStripMemberVisibility
ManifestParser::ParseTabStripMemberVisibility(const JSONValue* json_value) {
  if (!json_value) {
    return mojom::blink::TabStripMemberVisibility::kAuto;
  }

  String string_value;
  if (json_value->AsString(&string_value) &&
      EqualIgnoringASCIICase(string_value, "absent")) {
    return mojom::blink::TabStripMemberVisibility::kAbsent;
  }

  return mojom::blink::TabStripMemberVisibility::kAuto;
}

Vector<SafeUrlPattern> ManifestParser::ParseScopePatterns(
    const JSONObject* object) {
  Vector<SafeUrlPattern> result;

  if (!object->Get("scope_patterns")) {
    return result;
  }

  JSONArray* scope_patterns_list = object->GetArray("scope_patterns");
  if (!scope_patterns_list) {
    return result;
  }

  for (wtf_size_t i = 0; i < scope_patterns_list->size(); ++i) {
    // TODO(b/330640840): allow strings to be passed through here and parsed via
    // liburlpattern::ConstructorStringParser. The result of the parse can then
    // be used to create a PatternInit object for the rest of the process.
    JSONObject* pattern_object = JSONObject::Cast(scope_patterns_list->at(i));
    if (!pattern_object) {
      continue;
    }

    std::optional<PatternInit> init = MaybeCreatePatternInit(pattern_object);
    if (init.has_value()) {
      auto base_url = init->base_url.IsValid() ? init->base_url : manifest_url_;
      std::optional<SafeUrlPattern> pattern =
          ParseScopePattern(init.value(), base_url);
      if (pattern.has_value()) {
        result.push_back(std::move(pattern.value()));
      }
    }
  }

  return result;
}

std::optional<SafeUrlPattern> ManifestParser::ParseScopePattern(
    const PatternInit& init,
    const KURL& base_url) {
  auto url_pattern = std::make_optional<SafeUrlPattern>();

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Always fall back to baseURL protocol if init does not contain protocol.
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.protocol, base_url.Protocol());
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'protocol in home tab scope pattern could not be parsed or "
          "contains banned regex.");
      return std::nullopt;
    }
    url_pattern->protocol = std::move(part_list.value());
  }

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Only fall back to baseURL username if init does not contain any of
    // protocol, hostname, or port.
    String default_username;
    if (!init.IsAbsolute()) {
      default_username = base_url.User().ToString();
    }
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.username, default_username);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'username'in home tab scope pattern could not be parsed or "
          "contains banned regex.");
      return std::nullopt;
    }
    url_pattern->username = std::move(part_list.value());
  }

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Only fall back to baseURL password if init does not contain any of
    // protocol, hostname, port, or username.
    String default_password;
    if (!init.IsAbsolute() && !init.username.has_value()) {
      default_password = base_url.Pass().ToString();
    }
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.password, default_password);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'password' in home tab scope pattern could not be parsed "
          "or contains banned regex.");
      return std::nullopt;
    }
    url_pattern->password = std::move(part_list.value());
  }

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Only fall back to baseURL hostname if init does not contain protocol.
    String default_hostname;
    if (!init.protocol.has_value()) {
      default_hostname = base_url.Host().ToString();
    }
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.hostname, default_hostname);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'hostname' in home tab scope pattern could not be parsed "
          "or contains banned regex.");
      return std::nullopt;
    }
    url_pattern->hostname = std::move(part_list.value());
  }

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Only fall back to baseURL port if init does not contain any of
    // protocol, hostname, or port, and the baseURL port exists.
    String default_port;
    if (!init.IsAbsolute() && base_url.HasPort()) {
      default_port = String::Number(base_url.Port());
    }
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.port, default_port);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'port'in home tab scope pattern could not be parsed or "
          "contains banned regex.");
      return std::nullopt;
    }
    url_pattern->port = std::move(part_list.value());
  }

  {
    String default_path;
    if (init.pathname.has_value()) {
      // A possibly-relative path is given; resolve it against base URL's path.
      default_path =
          ResolveRelativePathnamePattern(base_url, init.pathname.value());
    } else if (!init.IsAbsolute()) {
      // No path, protocol, host or port is given; use the base URL's path.
      default_path = EscapePatternString(base_url.GetPath());
    }
    // else: no path, but a protocol, host or port was given, making this
    // pattern absolute, so treat the path as empty.

    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(std::nullopt, default_path);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'pathname'in home tab scope pattern could not be parsed or "
          "contains banned regex.");
      return std::nullopt;
    }
    url_pattern->pathname = std::move(part_list.value());
  }

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Only fall back to baseURL search if init does not contain any of
    // protocol, hostname, port, or pathname.
    String default_search;
    if (!init.IsAbsolute() && !init.pathname.has_value()) {
      default_search = base_url.Query().ToString();
    }
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.search, default_search);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'search' in home tab scope pattern could not be parsed "
          "or contains banned regex.");
      return std::nullopt;
    }
    url_pattern->search = std::move(part_list.value());
  }

  {
    // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
    // Only fall back to baseURL hash if init does not contain any of
    // protocol, hostname, port, pathname, or search.
    String default_hash;
    if (!init.IsAbsolute() && !init.pathname.has_value() &&
        !init.search.has_value()) {
      default_hash = base_url.FragmentIdentifier().ToString();
    }
    std::optional<std::vector<liburlpattern::Part>> part_list =
        ParsePatternInitField(init.hash, default_hash);
    if (!part_list.has_value()) {
      AddErrorInfo(
          "property 'hash' in home tab scope pattern could not be parsed "
          "or contains banned regex.");
      return std::nullopt;
    }
    url_pattern->hash = std::move(part_list.value());
  }

  return url_pattern;
}

std::optional<ManifestParser::PatternInit>
ManifestParser::MaybeCreatePatternInit(const JSONObject* pattern_object) {
  std::optional<String> protocol = ParseStringForMember(
      pattern_object, "scope_patterns", "protocol", false, Trim(true));
  std::optional<String> username = ParseStringForMember(
      pattern_object, "scope_patterns", "username", false, Trim(true));
  std::optional<String> password = ParseStringForMember(
      pattern_object, "scope_patterns", "password", false, Trim(true));
  std::optional<String> hostname = ParseStringForMember(
      pattern_object, "scope_patterns", "hostname", false, Trim(true));
  std::optional<String> port = ParseStringForMember(
      pattern_object, "scope_patterns", "port", false, Trim(true));
  std::optional<String> pathname = ParseStringForMember(
      pattern_object, "scope_patterns", "pathname", false, Trim(true));
  std::optional<String> search = ParseStringForMember(
      pattern_object, "scope_patterns", "search", false, Trim(true));
  std::optional<String> hash = ParseStringForMember(
      pattern_object, "scope_patterns", "hash", false, Trim(true));

  KURL base_url;

  if (pattern_object->Get("baseURL")) {
    base_url = ParseURL(pattern_object, "baseURL", KURL(),
                        ParseURLRestrictions::kNoRestrictions);
    if (!base_url.IsValid()) {
      return std::nullopt;
    }
  }

  return std::make_optional<PatternInit>(
      std::move(protocol), std::move(username), std::move(password),
      std::move(hostname), std::move(port), std::move(pathname),
      std::move(search), std::move(hash), base_url);
}

String ManifestParser::ParseVersion(const JSONObject* object) {
  return ParseString(object, "version", Trim(false)).value_or(String());
}

void ManifestParser::AddErrorInfo(const String& error_msg,
                                  bool critical,
                                  int error_line,
                                  int error_column) {
  mojom::blink::ManifestErrorPtr error = mojom::blink::ManifestError::New(
      error_msg, critical, error_line, error_column);
  errors_.push_back(std::move(error));
}

}  // namespace blink
