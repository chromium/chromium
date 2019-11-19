// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/manifest/manifest_parser.h"

#include "net/base/mime_util.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/platform/web_icon_sizes_parser.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/modules/manifest/manifest_uma_util.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool IsValidMimeType(const String& mime_type) {
  if (mime_type.StartsWith('.'))
    return true;
  return net::ParseMimeTypeWithoutParameter(mime_type.Utf8(), nullptr, nullptr);
}

bool VerifyFiles(const Vector<mojom::blink::ManifestFileFilterPtr>& files) {
  for (const auto& file : files) {
    for (const auto& accept_type : file->accept) {
      if (!IsValidMimeType(accept_type.LowerASCII()))
        return false;
    }
  }
  return true;
}

}  // anonymous namespace

ManifestParser::ManifestParser(const String& data,
                               const KURL& manifest_url,
                               const KURL& document_url)
    : data_(data),
      manifest_url_(manifest_url),
      document_url_(document_url),
      failed_(false) {}

ManifestParser::~ManifestParser() {}

void ManifestParser::Parse() {
  JSONParseError error;
  std::unique_ptr<JSONValue> root = ParseJSON(data_, &error);
  manifest_ = mojom::blink::Manifest::New();
  if (!root) {
    AddErrorInfo(error.message, true, error.line, error.column);
    ManifestUmaUtil::ParseFailed();
    failed_ = true;
    return;
  }

  std::unique_ptr<JSONObject> root_object = JSONObject::From(std::move(root));
  if (!root_object) {
    AddErrorInfo("root element must be a valid JSON object.", true);
    ManifestUmaUtil::ParseFailed();
    failed_ = true;
    return;
  }

  manifest_->name = ParseName(root_object.get());
  manifest_->short_name = ParseShortName(root_object.get());
  manifest_->start_url = ParseStartURL(root_object.get());
  manifest_->scope = ParseScope(root_object.get(), manifest_->start_url);
  manifest_->display = ParseDisplay(root_object.get());
  manifest_->orientation = ParseOrientation(root_object.get());
  manifest_->icons = ParseIcons(root_object.get());

  auto share_target = ParseShareTarget(root_object.get());
  if (share_target.has_value())
    manifest_->share_target = std::move(*share_target);

  auto file_handler = ParseFileHandler(root_object.get());
  if (file_handler.has_value())
    manifest_->file_handler = std::move(*file_handler);

  manifest_->related_applications = ParseRelatedApplications(root_object.get());
  manifest_->prefer_related_applications =
      ParsePreferRelatedApplications(root_object.get());

  base::Optional<RGBA32> theme_color = ParseThemeColor(root_object.get());
  manifest_->has_theme_color = theme_color.has_value();
  if (manifest_->has_theme_color)
    manifest_->theme_color = *theme_color;

  base::Optional<RGBA32> background_color =
      ParseBackgroundColor(root_object.get());
  manifest_->has_background_color = background_color.has_value();
  if (manifest_->has_background_color)
    manifest_->background_color = *background_color;

  manifest_->gcm_sender_id = ParseGCMSenderID(root_object.get());

  ManifestUmaUtil::ParseSucceeded(manifest_);
}

const mojom::blink::ManifestPtr& ManifestParser::manifest() const {
  return manifest_;
}

void ManifestParser::TakeErrors(
    Vector<mojom::blink::ManifestErrorPtr>* errors) {
  errors->clear();
  errors->swap(errors_);
}

bool ManifestParser::failed() const {
  return failed_;
}

bool ManifestParser::ParseBoolean(const JSONObject* object,
                                  const String& key,
                                  bool default_value) {
  JSONValue* json_value = object->Get(key);
  if (!json_value)
    return default_value;

  bool value;
  if (!json_value->AsBoolean(&value)) {
    AddErrorInfo("property '" + key + "' ignored, type " + "boolean expected.");
    return default_value;
  }

  return value;
}

base::Optional<String> ManifestParser::ParseString(const JSONObject* object,
                                                   const String& key,
                                                   TrimType trim) {
  JSONValue* json_value = object->Get(key);
  if (!json_value)
    return base::nullopt;

  String value;
  if (!json_value->AsString(&value)) {
    AddErrorInfo("property '" + key + "' ignored, type " + "string expected.");
    return base::nullopt;
  }

  if (trim == Trim)
    value = value.StripWhiteSpace();
  return value;
}

base::Optional<RGBA32> ManifestParser::ParseColor(const JSONObject* object,
                                                  const String& key) {
  base::Optional<String> parsed_color = ParseString(object, key, Trim);
  if (!parsed_color.has_value())
    return base::nullopt;

  Color color;
  if (!CSSParser::ParseColor(color, *parsed_color, true)) {
    AddErrorInfo("property '" + key + "' ignored, '" + *parsed_color +
                 "' is not a " + "valid color.");
    return base::nullopt;
  }

  return color.Rgb();
}

KURL ManifestParser::ParseURL(const JSONObject* object,
                              const String& key,
                              const KURL& base_url,
                              ParseURLOriginRestrictions origin_restriction) {
  base::Optional<String> url_str = ParseString(object, key, NoTrim);
  if (!url_str.has_value())
    return KURL();

  KURL resolved = KURL(base_url, *url_str);
  if (!resolved.IsValid()) {
    AddErrorInfo("property '" + key + "' ignored, URL is invalid.");
    return KURL();
  }

  switch (origin_restriction) {
    case ParseURLOriginRestrictions::kSameOriginOnly:
      if (!SecurityOrigin::AreSameSchemeHostPort(resolved, document_url_)) {
        AddErrorInfo("property '" + key +
                     "' ignored, should be same origin as document.");
        return KURL();
      }
      return resolved;
    case ParseURLOriginRestrictions::kNoRestrictions:
      return resolved;
  }

  NOTREACHED();
  return KURL();
}

String ManifestParser::ParseName(const JSONObject* object) {
  base::Optional<String> name = ParseString(object, "name", Trim);
  return name.has_value() ? *name : String();
}

String ManifestParser::ParseShortName(const JSONObject* object) {
  base::Optional<String> short_name = ParseString(object, "short_name", Trim);
  return short_name.has_value() ? *short_name : String();
}

KURL ManifestParser::ParseStartURL(const JSONObject* object) {
  return ParseURL(object, "start_url", manifest_url_,
                  ParseURLOriginRestrictions::kSameOriginOnly);
}

KURL ManifestParser::ParseScope(const JSONObject* object,
                                const KURL& start_url) {
  KURL scope = ParseURL(object, "scope", manifest_url_,
                        ParseURLOriginRestrictions::kSameOriginOnly);

  // This will change to remove the |document_url_| fallback in the future.
  // See https://github.com/w3c/manifest/issues/668.
  const KURL& default_value = start_url.IsEmpty() ? document_url_ : start_url;
  DCHECK(default_value.IsValid());

  if (scope.IsEmpty())
    return KURL(default_value.BaseAsString());

  if (!SecurityOrigin::AreSameSchemeHostPort(default_value, scope) ||
      !default_value.GetPath().StartsWith(scope.GetPath())) {
    AddErrorInfo(
        "property 'scope' ignored. Start url should be within scope "
        "of scope URL.");
    return KURL(default_value.BaseAsString());
  }

  DCHECK(scope.IsValid());
  return scope;
}

blink::mojom::DisplayMode ManifestParser::ParseDisplay(
    const JSONObject* object) {
  base::Optional<String> display = ParseString(object, "display", Trim);
  if (!display.has_value())
    return blink::mojom::DisplayMode::kUndefined;

  blink::mojom::DisplayMode display_enum =
      DisplayModeFromString(display->Utf8());
  if (display_enum == blink::mojom::DisplayMode::kUndefined)
    AddErrorInfo("unknown 'display' value ignored.");
  return display_enum;
}

WebScreenOrientationLockType ManifestParser::ParseOrientation(
    const JSONObject* object) {
  base::Optional<String> orientation = ParseString(object, "orientation", Trim);

  if (!orientation.has_value())
    return kWebScreenOrientationLockDefault;

  WebScreenOrientationLockType orientation_enum =
      WebScreenOrientationLockTypeFromString(orientation->Utf8());
  if (orientation_enum == kWebScreenOrientationLockDefault)
    AddErrorInfo("unknown 'orientation' value ignored.");
  return orientation_enum;
}

KURL ManifestParser::ParseIconSrc(const JSONObject* icon) {
  return ParseURL(icon, "src", manifest_url_,
                  ParseURLOriginRestrictions::kNoRestrictions);
}

String ManifestParser::ParseIconType(const JSONObject* icon) {
  base::Optional<String> type = ParseString(icon, "type", Trim);
  return type.has_value() ? *type : String("");
}

Vector<WebSize> ManifestParser::ParseIconSizes(const JSONObject* icon) {
  base::Optional<String> sizes_str = ParseString(icon, "sizes", NoTrim);
  if (!sizes_str.has_value())
    return Vector<WebSize>();

  WebVector<WebSize> web_sizes =
      WebIconSizesParser::ParseIconSizes(WebString(*sizes_str));
  Vector<WebSize> sizes;
  for (auto& size : web_sizes)
    sizes.push_back(size);

  if (sizes.IsEmpty())
    AddErrorInfo("found icon with no valid size.");
  return sizes;
}

base::Optional<Vector<mojom::blink::ManifestImageResource::Purpose>>
ManifestParser::ParseIconPurpose(const JSONObject* icon) {
  base::Optional<String> purpose_str = ParseString(icon, "purpose", NoTrim);
  Vector<mojom::blink::ManifestImageResource::Purpose> purposes;

  if (!purpose_str.has_value()) {
    purposes.push_back(mojom::blink::ManifestImageResource::Purpose::ANY);
    return purposes;
  }

  Vector<String> keywords;
  purpose_str.value().Split(/*separator=*/" ", /*allow_empty_entries=*/false,
                            keywords);

  // "any" is the default if there are no other keywords.
  if (keywords.IsEmpty()) {
    purposes.push_back(mojom::blink::ManifestImageResource::Purpose::ANY);
    return purposes;
  }

  bool unrecognised_purpose = false;
  for (auto& keyword : keywords) {
    keyword = keyword.StripWhiteSpace();
    if (keyword.IsEmpty())
      continue;

    if (!CodeUnitCompareIgnoringASCIICase(keyword, "any")) {
      purposes.push_back(mojom::blink::ManifestImageResource::Purpose::ANY);
    } else if (!CodeUnitCompareIgnoringASCIICase(keyword, "badge")) {
      purposes.push_back(mojom::blink::ManifestImageResource::Purpose::BADGE);
    } else if (!CodeUnitCompareIgnoringASCIICase(keyword, "maskable")) {
      purposes.push_back(
          mojom::blink::ManifestImageResource::Purpose::MASKABLE);
    } else {
      unrecognised_purpose = true;
    }
  }

  // This implies there was at least one purpose given, but none recognised.
  // Instead of defaulting to "any" (which would not be future proof),
  // invalidate the whole icon.
  if (purposes.IsEmpty()) {
    AddErrorInfo("found icon with no valid purpose; ignoring it.");
    return base::nullopt;
  }

  if (unrecognised_purpose) {
    AddErrorInfo(
        "found icon with one or more invalid purposes; those purposes are "
        "ignored.");
  }

  return purposes;
}

Vector<mojom::blink::ManifestImageResourcePtr> ManifestParser::ParseIcons(
    const JSONObject* object) {
  Vector<mojom::blink::ManifestImageResourcePtr> icons;
  JSONValue* json_value = object->Get("icons");
  if (!json_value)
    return icons;

  JSONArray* icons_list = object->GetArray("icons");
  if (!icons_list) {
    AddErrorInfo("property 'icons' ignored, type array expected.");
    return icons;
  }

  for (wtf_size_t i = 0; i < icons_list->size(); ++i) {
    JSONObject* icon_object = JSONObject::Cast(icons_list->at(i));
    if (!icon_object)
      continue;

    auto icon = mojom::blink::ManifestImageResource::New();
    icon->src = ParseIconSrc(icon_object);
    // An icon MUST have a valid src. If it does not, it MUST be ignored.
    if (!icon->src.IsValid())
      continue;

    icon->type = ParseIconType(icon_object);
    icon->sizes = ParseIconSizes(icon_object);
    auto purpose = ParseIconPurpose(icon_object);
    if (!purpose)
      continue;

    icon->purpose = std::move(*purpose);

    icons.push_back(std::move(icon));
  }

  return icons;
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
  if (!object->Get("accept"))
    return accept_types;

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
  if (!from->Get(key))
    return files;

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
  if (file->name.IsEmpty()) {
    // https://wicg.github.io/web-share-target/level-2/#share_target-member
    // step 7.1 requires that we invalidate this FileFilter if 'name' is an
    // empty string. We also invalidate if 'name' is undefined or not a
    // string.
    return;
  }

  file->accept = ParseFileFilterAccept(file_object);
  if (file->accept.IsEmpty())
    return;

  files->push_back(std::move(file));
}

base::Optional<mojom::blink::ManifestShareTarget::Method>
ManifestParser::ParseShareTargetMethod(const JSONObject* share_target_object) {
  if (!share_target_object->Get("method")) {
    AddErrorInfo(
        "Method should be set to either GET or POST. It currently defaults to "
        "GET.");
    return mojom::blink::ManifestShareTarget::Method::kGet;
  }

  String value;
  if (!share_target_object->GetString("method", &value))
    return base::nullopt;

  String method = value.UpperASCII();
  if (method == "GET")
    return mojom::blink::ManifestShareTarget::Method::kGet;
  if (method == "POST")
    return mojom::blink::ManifestShareTarget::Method::kPost;

  return base::nullopt;
}

base::Optional<mojom::blink::ManifestShareTarget::Enctype>
ManifestParser::ParseShareTargetEnctype(const JSONObject* share_target_object) {
  if (!share_target_object->Get("enctype")) {
    AddErrorInfo(
        "Enctype should be set to either application/x-www-form-urlencoded or "
        "multipart/form-data. It currently defaults to "
        "application/x-www-form-urlencoded");
    return mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded;
  }

  String value;
  if (!share_target_object->GetString("enctype", &value))
    return base::nullopt;

  String enctype = value.LowerASCII();
  if (enctype == "application/x-www-form-urlencoded")
    return mojom::blink::ManifestShareTarget::Enctype::kFormUrlEncoded;

  if (enctype == "multipart/form-data")
    return mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData;

  return base::nullopt;
}

mojom::blink::ManifestShareTargetParamsPtr
ManifestParser::ParseShareTargetParams(const JSONObject* share_target_params) {
  auto params = mojom::blink::ManifestShareTargetParams::New();

  // NOTE: These are key names for query parameters, which are filled with share
  // data. As such, |params.url| is just a string.
  base::Optional<String> text = ParseString(share_target_params, "text", Trim);
  params->text = text.has_value() ? *text : String();
  base::Optional<String> title =
      ParseString(share_target_params, "title", Trim);
  params->title = title.has_value() ? *title : String();
  base::Optional<String> url = ParseString(share_target_params, "url", Trim);
  params->url = url.has_value() ? *url : String();

  auto files = ParseTargetFiles("files", share_target_params);
  if (!files.IsEmpty())
    params->files = std::move(files);
  return params;
}

base::Optional<mojom::blink::ManifestShareTargetPtr>
ManifestParser::ParseShareTarget(const JSONObject* object) {
  const JSONObject* share_target_object = object->GetJSONObject("share_target");
  if (!share_target_object)
    return base::nullopt;

  auto share_target = mojom::blink::ManifestShareTarget::New();
  share_target->action = ParseURL(share_target_object, "action", manifest_url_,
                                  ParseURLOriginRestrictions::kSameOriginOnly);
  if (!share_target->action.IsValid()) {
    AddErrorInfo(
        "property 'share_target' ignored. Property 'action' is "
        "invalid.");
    return base::nullopt;
  }

  auto method = ParseShareTargetMethod(share_target_object);
  auto enctype = ParseShareTargetEnctype(share_target_object);

  const JSONObject* share_target_params_object =
      share_target_object->GetJSONObject("params");
  if (!share_target_params_object) {
    AddErrorInfo(
        "property 'share_target' ignored. Property 'params' type "
        "dictionary expected.");
    return base::nullopt;
  }

  share_target->params = ParseShareTargetParams(share_target_params_object);
  if (!method.has_value()) {
    AddErrorInfo(
        "invalid method. Allowed methods are:"
        "GET and POST.");
    return base::nullopt;
  }
  share_target->method = method.value();

  if (!enctype.has_value()) {
    AddErrorInfo(
        "invalid enctype. Allowed enctypes are:"
        "application/x-www-form-urlencoded and multipart/form-data.");
    return base::nullopt;
  }
  share_target->enctype = enctype.value();

  if (share_target->method == mojom::blink::ManifestShareTarget::Method::kGet) {
    if (share_target->enctype ==
        mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData) {
      AddErrorInfo(
          "invalid enctype for GET method. Only "
          "application/x-www-form-urlencoded is allowed.");
      return base::nullopt;
    }
  }

  if (share_target->params->files.has_value()) {
    if (share_target->method !=
            mojom::blink::ManifestShareTarget::Method::kPost ||
        share_target->enctype !=
            mojom::blink::ManifestShareTarget::Enctype::kMultipartFormData) {
      AddErrorInfo("files are only supported with multipart/form-data POST.");
      return base::nullopt;
    }
  }

  if (share_target->params->files.has_value() &&
      !VerifyFiles(*share_target->params->files)) {
    AddErrorInfo("invalid mime type inside files.");
    return base::nullopt;
  }

  return share_target;
}

base::Optional<mojom::blink::ManifestFileHandlerPtr>
ManifestParser::ParseFileHandler(const JSONObject* object) {
  const JSONObject* file_handler_object = object->GetJSONObject("file_handler");
  if (!file_handler_object)
    return base::nullopt;

  auto file_handler = mojom::blink::ManifestFileHandler::New();
  file_handler->action = ParseURL(file_handler_object, "action", manifest_url_,
                                  ParseURLOriginRestrictions::kSameOriginOnly);
  if (!file_handler->action.IsValid()) {
    AddErrorInfo(
        "property 'file_handler' ignored. Property 'action' is "
        "invalid.");
    return base::nullopt;
  }

  file_handler->files = ParseTargetFiles("files", file_handler_object);

  if (file_handler->files.size() == 0) {
    AddErrorInfo("no file handlers were specified.");
    return base::nullopt;
  }

  return file_handler;
}

String ManifestParser::ParseRelatedApplicationPlatform(
    const JSONObject* application) {
  base::Optional<String> platform = ParseString(application, "platform", Trim);
  return platform.has_value() ? *platform : String();
}

base::Optional<KURL> ManifestParser::ParseRelatedApplicationURL(
    const JSONObject* application) {
  return ParseURL(application, "url", manifest_url_,
                  ParseURLOriginRestrictions::kNoRestrictions);
}

String ManifestParser::ParseRelatedApplicationId(
    const JSONObject* application) {
  base::Optional<String> id = ParseString(application, "id", Trim);
  return id.has_value() ? *id : String();
}

Vector<mojom::blink::ManifestRelatedApplicationPtr>
ManifestParser::ParseRelatedApplications(const JSONObject* object) {
  Vector<mojom::blink::ManifestRelatedApplicationPtr> applications;

  JSONValue* value = object->Get("related_applications");
  if (!value)
    return applications;

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
    if (!application_object)
      continue;

    auto application = mojom::blink::ManifestRelatedApplication::New();
    application->platform = ParseRelatedApplicationPlatform(application_object);
    // "If platform is undefined, move onto the next item if any are left."
    if (application->platform.IsEmpty()) {
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
        application->id.IsEmpty()) {
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

base::Optional<RGBA32> ManifestParser::ParseThemeColor(
    const JSONObject* object) {
  return ParseColor(object, "theme_color");
}

base::Optional<RGBA32> ManifestParser::ParseBackgroundColor(
    const JSONObject* object) {
  return ParseColor(object, "background_color");
}

String ManifestParser::ParseGCMSenderID(const JSONObject* object) {
  base::Optional<String> gcm_sender_id =
      ParseString(object, "gcm_sender_id", Trim);
  return gcm_sender_id.has_value() ? *gcm_sender_id : String();
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
