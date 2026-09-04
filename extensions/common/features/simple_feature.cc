// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_developer_mode_only.h"
#include "extensions/common/features/feature_flags.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/feature_session_type.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"

using crx_file::id_util::HashedIdInHex;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

struct AllowlistInfo {
  AllowlistInfo() {
    const std::string& allowlisted_extension_ids =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAllowlistedExtensionID);
    for (const auto& id :
         base::SplitString(allowlisted_extension_ids, ",",
                           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
      hashed_ids.push_back(HashedIdInHex(id));
    }
  }
  std::vector<std::string> hashed_ids;
};

// A singleton copy of the --allowlisted-extension-id so that we don't need to
// copy it from the CommandLine each time.
AllowlistInfo& GetAllowlistInfo() {
  static base::NoDestructor<AllowlistInfo> instance;
  return *instance;
}

Feature::Availability IsAvailableToManifestForBind(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    ManifestLocation location,
    int manifest_version,
    Feature::Platform platform,
    int context_id,
    const Feature* feature) {
  return feature->IsAvailableToManifest(hashed_id, type, location,
                                        manifest_version, platform);
}

Feature::Availability IsAvailableToEnvironmentForBind(int context_id,
                                                      const Feature* feature) {
  return feature->IsAvailableToEnvironment(context_id);
}

// Gets a human-readable name for the given extension type, suitable for giving
// to developers in an error message.
std::string_view GetDisplayName(Manifest::Type type) {
  switch (type) {
    case Manifest::Type::kUnknown:
      return "unknown";
    case Manifest::Type::kExtension:
      return "extension";
    case Manifest::Type::kHostedApp:
      return "hosted app";
    case Manifest::Type::kLegacyPackagedApp:
      return "legacy packaged app";
    case Manifest::Type::kPlatformApp:
      return "packaged app";
    case Manifest::Type::kTheme:
      return "theme";
    case Manifest::Type::kUserScript:
      return "user script";
    case Manifest::Type::kSharedModule:
      return "shared module";
    case Manifest::Type::kLoginScreenExtension:
      return "login screen extension";
    case Manifest::Type::kChromeOSSystemExtension:
      return "chromeos system extension";
    case Manifest::Type::kNumLoadTypes:
      NOTREACHED();
  }
  NOTREACHED();
}

// Gets a human-readable name for the given context type, suitable for giving
// to developers in an error message.
std::string_view GetDisplayName(mojom::ContextType context) {
  switch (context) {
    case mojom::ContextType::kUnspecified:
      return "unknown";
    case mojom::ContextType::kPrivilegedExtension:
      // "privileged" is vague but hopefully the developer will understand that
      // means background or app window.
      return "privileged page";
    case mojom::ContextType::kUnprivilegedExtension:
      // "iframe" is a bit of a lie/oversimplification, but that's the most
      // common unblessed context.
      return "extension iframe";
    case mojom::ContextType::kContentScript:
      return "content script";
    case mojom::ContextType::kWebPage:
      return "web page";
    case mojom::ContextType::kPrivilegedWebPage:
      return "hosted app";
    case mojom::ContextType::kWebUi:
      return "webui";
    case mojom::ContextType::kUntrustedWebUi:
      return "webui untrusted";
    case mojom::ContextType::kOffscreenExtension:
      return "offscreen document";
    case mojom::ContextType::kUserScript:
      return "user script";
  }
  NOTREACHED();
}

std::string_view GetDisplayName(mojom::FeatureSessionType session_type) {
  switch (session_type) {
    case mojom::FeatureSessionType::kInitial:
      return "user-less";
    case mojom::FeatureSessionType::kUnknown:
      return "unknown";
    case mojom::FeatureSessionType::kKiosk:
      return "kiosk app";
    case mojom::FeatureSessionType::kAutolaunchedKiosk:
      return "auto-launched kiosk app";
    case mojom::FeatureSessionType::kRegular:
      return "regular user";
  }
  return "";
}

// Gets a human-readable list of the display names (pluralized, comma separated
// with the "and" in the correct place) for each of |enum_types|.
template <typename EnumType, size_t Extent, typename InternalPtrType>
std::string ListDisplayNames(
    base::span<const EnumType, Extent, InternalPtrType> enum_types) {
  std::string display_name_list;
  for (size_t i = 0; i < enum_types.size(); ++i) {
    // Pluralize type name.
    base::StrAppend(&display_name_list, {GetDisplayName(enum_types[i]), "s"});
    // Comma-separate entries, with an Oxford comma if there is more than 2
    // total entries.
    if (enum_types.size() > 2) {
      if (i < enum_types.size() - 2)
        display_name_list += ", ";
      else if (i == enum_types.size() - 2)
        display_name_list += ", and ";
    } else if (enum_types.size() == 2 && i == 0) {
      display_name_list += " and ";
    }
  }
  return display_name_list;
}

bool IsCommandLineSwitchEnabled(base::CommandLine* command_line,
                                std::string_view switch_name) {
  if (command_line->GetSwitchValueASCII(switch_name) == "1") {
    return true;
  }
  if (command_line->HasSwitch(base::StrCat({"enable-", switch_name}))) {
    return true;
  }
  return false;
}

bool IsAllowlistedForTest(const HashedExtensionId& hashed_id) {
  const auto& ids = GetAllowlistInfo().hashed_ids;
  return std::ranges::contains(ids, hashed_id.value());
}

}  // namespace

SimpleFeature::ScopedThreadUnsafeAllowlistForTest::
    ScopedThreadUnsafeAllowlistForTest(const std::string& id)
    : previous_ids_(GetAllowlistInfo().hashed_ids) {
  GetAllowlistInfo().hashed_ids = {HashedIdInHex(id)};
}

SimpleFeature::ScopedThreadUnsafeAllowlistForTest::
    ScopedThreadUnsafeAllowlistForTest(const std::vector<std::string>& ids)
    : previous_ids_(GetAllowlistInfo().hashed_ids) {
  GetAllowlistInfo().hashed_ids.clear();
  for (const auto& id : ids) {
    GetAllowlistInfo().hashed_ids.push_back(HashedIdInHex(id));
  }
}

// static
std::unique_ptr<SimpleFeature::ScopedThreadUnsafeAllowlistForTest>
SimpleFeature::ScopedThreadUnsafeAllowlistForTest::CreateFromCommaSeparated(
    const std::string& comma_separated_ids) {
  return std::make_unique<ScopedThreadUnsafeAllowlistForTest>(
      base::SplitString(comma_separated_ids, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY));
}

SimpleFeature::ScopedThreadUnsafeAllowlistForTest::
    ~ScopedThreadUnsafeAllowlistForTest() {
  GetAllowlistInfo().hashed_ids = previous_ids_;
}

SimpleFeature::SimpleFeature(StaticFeatureData<SimpleFeatureData> data)
    : SimpleFeature(data.get()) {}

SimpleFeature::SimpleFeature(const SimpleFeatureData* data)
    : Feature(&CHECK_DEREF(data).feature),
      simple_feature_config_(&data->config) {}

SimpleFeature::~SimpleFeature() = default;

base::span<const std::string_view> SimpleFeature::blocklist() const {
  return simple_feature_config_->blocklist.span();
}

base::span<const std::string_view> SimpleFeature::allowlist() const {
  return simple_feature_config_->allowlist.span();
}

base::span<const Manifest::Type> SimpleFeature::extension_types() const {
  return simple_feature_config_->extension_types.span();
}

base::span<const Feature::Platform> SimpleFeature::platforms() const {
  return simple_feature_config_->platforms.span();
}

std::optional<base::span<const mojom::ContextType>> SimpleFeature::contexts()
    const {
  if (!simple_feature_config_->contexts) {
    return std::nullopt;
  }
  return simple_feature_config_->contexts->span();
}

base::span<const std::string_view> SimpleFeature::dependencies() const {
  return simple_feature_config_->dependencies.span();
}

std::optional<version_info::Channel> SimpleFeature::channel() const {
  return simple_feature_config_->channel;
}

std::optional<SimpleFeature::Location> SimpleFeature::location() const {
  return simple_feature_config_->location;
}

std::optional<int> SimpleFeature::min_manifest_version() const {
  return simple_feature_config_->min_manifest_version;
}

std::optional<int> SimpleFeature::max_manifest_version() const {
  return simple_feature_config_->max_manifest_version;
}

std::optional<std::string_view> SimpleFeature::command_line_switch() const {
  const StaticCString switch_name = command_line_switch_data();
  return switch_name.has_value() ? std::optional(switch_name.string_view())
                                 : std::nullopt;
}

bool SimpleFeature::component_extensions_auto_granted() const {
  return simple_feature_config_->component_extensions_auto_granted;
}

base::span<const std::string_view> SimpleFeature::match_patterns() const {
  return simple_feature_config_->match_patterns.span();
}

base::span<const mojom::FeatureSessionType> SimpleFeature::session_types()
    const {
  return simple_feature_config_->session_types.span();
}

StaticCString SimpleFeature::command_line_switch_data() const {
  return simple_feature_config_->command_line_switch;
}

StaticCString SimpleFeature::feature_flag() const {
  return simple_feature_config_->feature_flag;
}

bool SimpleFeature::developer_mode_only() const {
  return simple_feature_config_->developer_mode_only;
}

bool SimpleFeature::disallow_for_service_workers() const {
  return simple_feature_config_->disallow_for_service_workers;
}

Feature::Availability SimpleFeature::IsAvailableToManifest(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    ManifestLocation location,
    int manifest_version,
    Platform platform,
    int context_id) const {
  Availability environment_availability = GetEnvironmentAvailability(
      platform, GetCurrentChannel(), GetCurrentFeatureSessionType(), context_id,
      true);
  if (!environment_availability.is_available())
    return environment_availability;
  Availability manifest_availability =
      GetManifestAvailability(hashed_id, type, location, manifest_version);
  if (!manifest_availability.is_available())
    return manifest_availability;

  // Avoid allocating the dependency-check callback in the common
  // (no-dependency) case.
  if (dependencies().empty()) {
    return CreateAvailability(AvailabilityResult::kIsAvailable);
  }

  return CheckDependencies(
      base::BindRepeating(&IsAvailableToManifestForBind, hashed_id, type,
                          location, manifest_version, platform, context_id));
}

Feature::Availability SimpleFeature::IsAvailableToContextForBind(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    Feature::Platform platform,
    int context_id,
    const ContextData* context_data,
    const Feature* feature) {
  CHECK(feature);
  CHECK(context_data);
  return feature->IsAvailableToContextImpl(extension, context, url, platform,
                                           context_id, true, *context_data);
}

Feature::Availability SimpleFeature::IsAvailableToContextImpl(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    Platform platform,
    int context_id,
    bool check_developer_mode,
    const ContextData& context_data) const {
  Availability environment_availability = GetEnvironmentAvailability(
      platform, GetCurrentChannel(), GetCurrentFeatureSessionType(), context_id,
      check_developer_mode);
  if (!environment_availability.is_available())
    return environment_availability;

  if (RequiresDelegatedAvailabilityCheck()) {
    Feature::Availability delegated_availibility =
        HasDelegatedAvailabilityCheckHandler()
            ? RunDelegatedAvailabilityCheck(extension, context, url, platform,
                                            context_id, check_developer_mode,
                                            context_data)
            : CreateAvailability(
                  AvailabilityResult::kMissingDelegatedAvailabilityCheck);

    if (!delegated_availibility.is_available()) {
      return delegated_availibility;
    }
  }

  if (extension) {
    Availability manifest_availability = GetManifestAvailability(
        extension->hashed_id(), extension->GetType(), extension->location(),
        extension->manifest_version());
    if (!manifest_availability.is_available())
      return manifest_availability;
  }

  bool is_for_service_worker =
      extension && BackgroundInfo::IsServiceWorkerBased(extension) &&
      url.is_valid() &&
      url == BackgroundInfo::GetBackgroundServiceWorkerScriptURL(extension);

  Availability context_availability =
      GetContextAvailability(context, url, is_for_service_worker);
  if (!context_availability.is_available())
    return context_availability;

  // TODO(kalman): Assert that if the context was a webpage or WebUI context
  // then at some point a "matches" restriction was checked.

  // Avoid allocating the dependency-check callback in the common
  // (no-dependency) case.
  if (dependencies().empty()) {
    return CreateAvailability(AvailabilityResult::kIsAvailable);
  }

  return CheckDependencies(base::BindRepeating(
      &IsAvailableToContextForBind, base::RetainedRef(extension), context, url,
      platform, context_id, base::Unretained(&context_data)));
}

Feature::Availability SimpleFeature::IsAvailableToEnvironment(
    int context_id) const {
  Availability environment_availability = GetEnvironmentAvailability(
      GetCurrentPlatform(), GetCurrentChannel(), GetCurrentFeatureSessionType(),
      context_id, true);
  if (!environment_availability.is_available())
    return environment_availability;

  // Avoid allocating the dependency-check callback in the common
  // (no-dependency) case.
  if (dependencies().empty()) {
    return CreateAvailability(AvailabilityResult::kIsAvailable);
  }

  return CheckDependencies(
      base::BindRepeating(&IsAvailableToEnvironmentForBind, context_id));
}

std::string SimpleFeature::GetAvailabilityMessage(
    AvailabilityResult result,
    Manifest::Type type,
    const GURL& url,
    mojom::ContextType context,
    version_info::Channel channel,
    mojom::FeatureSessionType session_type) const {
  switch (result) {
    case AvailabilityResult::kIsAvailable:
      return std::string();
    case AvailabilityResult::kNotFoundInAllowlist:
    case AvailabilityResult::kFoundInBlocklist:
      return base::StringPrintf(
          "'%s' is not allowed for specified extension ID.", name());
    case AvailabilityResult::kInvalidUrl:
      return base::StringPrintf("'%s' is not allowed on %s.", name(),
                                url.spec());
    case AvailabilityResult::kInvalidType: {
      const auto types = extension_types();
      return base::StringPrintf(
          "'%s' is only allowed for %s, but this is a %s.", name(),
          ListDisplayNames(types), GetDisplayName(type));
    }
    case AvailabilityResult::kInvalidContext: {
      const auto allowed_contexts = contexts();
      DCHECK(allowed_contexts);
      return base::StringPrintf(
          "'%s' is only allowed to run in %s, but this is a %s", name(),
          ListDisplayNames(*allowed_contexts), GetDisplayName(context));
    }
    case AvailabilityResult::kInvalidLocation:
      return base::StringPrintf(
          "'%s' is not allowed for specified install location.", name());
    case AvailabilityResult::kInvalidPlatform:
      return base::StringPrintf("'%s' is not allowed for specified platform.",
                                name());
    case AvailabilityResult::kInvalidMinManifestVersion: {
      const auto min_version = min_manifest_version();
      DCHECK(min_version);
      return base::StringPrintf(
          "'%s' requires manifest version of at least %d.", name(),
          *min_version);
    }
    case AvailabilityResult::kInvalidMaxManifestVersion: {
      const auto max_version = max_manifest_version();
      DCHECK(max_version);
      return base::StringPrintf(
          "'%s' requires manifest version of %d or lower.", name(),
          *max_version);
    }
    case AvailabilityResult::kInvalidSessionType: {
      const auto types = session_types();
      return base::StringPrintf(
          "'%s' is only allowed to run in %s sessions, but this is %s session.",
          name(), ListDisplayNames(types), GetDisplayName(session_type));
    }
    case AvailabilityResult::kNotPresent:
      return base::StringPrintf(
          "'%s' requires a different Feature that is not present.", name());
    case AvailabilityResult::kUnsupportedChannel:
      return base::StringPrintf(
          "'%s' requires %s channel or newer, but this is the %s channel.",
          name(), version_info::GetChannelString(channel),
          version_info::GetChannelString(GetCurrentChannel()));
    case AvailabilityResult::kMissingCommandLineSwitch: {
      const StaticCString switch_name = command_line_switch_data();
      DCHECK(switch_name.has_value());
      return base::StringPrintf(
          "'%s' requires the '%s' command line switch to be enabled.", name(),
          switch_name.string_view());
    }
    case AvailabilityResult::kFeatureFlagDisabled: {
      const StaticCString flag = feature_flag();
      DCHECK(flag.has_value());
      return base::StringPrintf(
          "'%s' requires the '%s' feature flag to be enabled.", name(),
          flag.string_view());
    }
    case AvailabilityResult::kRequiresDeveloperMode:
      return base::StringPrintf(
          "'%s' requires the user to have developer mode enabled.", name());
    case AvailabilityResult::kMissingDelegatedAvailabilityCheck:
      return base::StringPrintf(
          "'%s' is missing its delegated availability check", name());
    case AvailabilityResult::kFailedDelegatedAvailabilityCheck:
      return base::StringPrintf("'%s' failed its delegated availability check.",
                                name());
  }

  NOTREACHED();
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::Type::kUnknown, GURL(),
                                     mojom::ContextType::kUnspecified,
                                     version_info::Channel::UNKNOWN,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result, Manifest::Type type) const {
  return Availability(
      result, GetAvailabilityMessage(result, type, GURL(),
                                     mojom::ContextType::kUnspecified,
                                     version_info::Channel::UNKNOWN,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    const GURL& url) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::Type::kUnknown, url,
                                     mojom::ContextType::kUnspecified,
                                     version_info::Channel::UNKNOWN,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    mojom::ContextType context) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::Type::kUnknown, GURL(),
                                     context, version_info::Channel::UNKNOWN,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    version_info::Channel channel) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::Type::kUnknown, GURL(),
                                     mojom::ContextType::kUnspecified, channel,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    mojom::FeatureSessionType session_type) const {
  return Availability(
      result,
      GetAvailabilityMessage(result, Manifest::Type::kUnknown, GURL(),
                             mojom::ContextType::kUnspecified,
                             version_info::Channel::UNKNOWN, session_type));
}

bool SimpleFeature::IsInternal() const {
  return simple_feature_config_->is_internal;
}

bool SimpleFeature::IsIdInBlocklist(const HashedExtensionId& hashed_id) const {
  return IsIdInList(hashed_id, blocklist());
}

bool SimpleFeature::IsIdInAllowlist(const HashedExtensionId& hashed_id) const {
  return IsIdInList(hashed_id, allowlist());
}

// static
bool SimpleFeature::IsIdInList(const HashedExtensionId& hashed_id,
                               base::span<const std::string_view> list) {
  if (!IsValidHashedExtensionId(hashed_id))
    return false;

  // TODO(crbug.com/455599844): Remove the fallback search on `value_sha1()` and
  // query only `value_sha256()` once the SHA-256 rollout is 100% complete and
  // the feature is enabled by default.
  return std::ranges::contains(list, hashed_id.value_sha256()) ||
         std::ranges::contains(list, hashed_id.value_sha1());
}

bool SimpleFeature::MatchesManifestLocation(
    ManifestLocation manifest_location) const {
  const auto required_location = location();
  DCHECK(required_location);
  switch (*required_location) {
    case SimpleFeature::Location::kComponent:
      return manifest_location == ManifestLocation::kComponent;
    case SimpleFeature::Location::kExternalComponent:
      return manifest_location == ManifestLocation::kExternalComponent;
    case SimpleFeature::Location::kPolicy:
      return manifest_location == ManifestLocation::kExternalPolicy ||
             manifest_location == ManifestLocation::kExternalPolicyDownload;
    case SimpleFeature::Location::kUnpacked:
      return Manifest::IsUnpackedLocation(manifest_location);
  }
  NOTREACHED();
}

bool SimpleFeature::MatchesSessionTypes(
    mojom::FeatureSessionType session_type) const {
  const auto allowed_session_types = session_types();
  if (allowed_session_types.empty()) {
    return true;
  }

  if (std::ranges::contains(allowed_session_types, session_type)) {
    return true;
  }

  // AUTOLAUNCHED_KIOSK session type is subset of KIOSK - accept auto-lauched
  // kiosk session if kiosk session is allowed. This is the only exception to
  // rejecting a session type that is not present in `allowed_session_types`.
  return session_type == mojom::FeatureSessionType::kAutolaunchedKiosk &&
         std::ranges::contains(allowed_session_types,
                               mojom::FeatureSessionType::kKiosk);
}

bool SimpleFeature::RequiresDelegatedAvailabilityCheck() const {
  return simple_feature_config_->requires_delegated_availability_check;
}

bool SimpleFeature::HasDelegatedAvailabilityCheckHandler() const {
  return !delegated_availability_check_handler_.is_null();
}

void SimpleFeature::SetDelegatedAvailabilityCheckHandler(
    DelegatedAvailabilityCheckHandler handler) {
  DCHECK(RequiresDelegatedAvailabilityCheck());
  DCHECK(!HasDelegatedAvailabilityCheckHandler());
  delegated_availability_check_handler_ = std::move(handler);
}

Feature::Availability SimpleFeature::CheckDependencies(
    const base::RepeatingCallback<Availability(const Feature*)>& checker)
    const {
  for (const auto& dep_name : dependencies()) {
    const Feature* dependency =
        ExtensionAPI::GetSharedInstance()->GetFeatureDependency(dep_name);
    if (!dependency)
      return CreateAvailability(AvailabilityResult::kNotPresent);
    Availability dependency_availability = checker.Run(dependency);
    if (!dependency_availability.is_available())
      return dependency_availability;
  }
  return CreateAvailability(AvailabilityResult::kIsAvailable);
}

// static
bool SimpleFeature::IsValidExtensionId(const ExtensionId& extension_id) {
  // Belt-and-suspenders philosophy here. We should be pretty confident by this
  // point that we've validated the extension ID format, but in case something
  // slips through, we avoid a class of attack where creative ID manipulation
  // leads to hash collisions.
  // 128 bits / 4 = 32 mpdecimal characters
  return (extension_id.length() == 32);
}

// static
bool SimpleFeature::IsValidHashedExtensionId(
    const HashedExtensionId& hashed_id) {
  // TODO(crbug.com/455599844): Remove the 40-character length check (SHA-1) and
  // require strictly 64 characters (SHA-256) once the rollout is complete.
  // Allow both 40-character (SHA-1) and 64-character (SHA-256) hashes.
  return hashed_id.value().length() == 40 || hashed_id.value().length() == 64;
}

bool SimpleFeature::MatchesURL(const GURL& url) const {
  // Create the URLPattern per call to avoid the memory overhead of storing it
  // for the feature's process lifetime.
  return std::ranges::any_of(
      match_patterns(), [&url](std::string_view pattern) {
        return URLPattern(URLPattern::SCHEME_ALL, pattern).MatchesURL(url);
      });
}

Feature::Availability SimpleFeature::GetEnvironmentAvailability(
    Platform platform,
    version_info::Channel channel,
    mojom::FeatureSessionType session_type,
    int context_id,
    bool check_developer_mode) const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const auto allowed_platforms = platforms();
  if (!allowed_platforms.empty() &&
      !std::ranges::contains(allowed_platforms, platform)) {
    return CreateAvailability(AvailabilityResult::kInvalidPlatform);
  }

  const auto required_channel = this->channel();
  if (required_channel && *required_channel < GetCurrentChannel()) {
    // If the user has the kEnableExperimentalExtensionApis commandline flag
    // appended, we ignore channel restrictions.
    if (!command_line->HasSwitch(switches::kEnableExperimentalExtensionApis)) {
      return CreateAvailability(AvailabilityResult::kUnsupportedChannel,
                                *required_channel);
    }
  }

  const StaticCString required_switch = command_line_switch_data();
  if (required_switch.has_value() &&
      !IsCommandLineSwitchEnabled(command_line,
                                  required_switch.string_view())) {
    return CreateAvailability(AvailabilityResult::kMissingCommandLineSwitch);
  }

  const StaticCString required_flag = feature_flag();
  if (required_flag.has_value() &&
      !IsFeatureFlagEnabled(required_flag.string_view())) {
    return CreateAvailability(AvailabilityResult::kFeatureFlagDisabled);
  }

  if (!MatchesSessionTypes(session_type))
    return CreateAvailability(AvailabilityResult::kInvalidSessionType,
                              session_type);

  bool debugger_api_restricted = base::FeatureList::IsEnabled(
      extensions_features::kDebuggerAPIRestrictedToDevMode);

  if (check_developer_mode && developer_mode_only() &&
      !GetCurrentDeveloperMode(context_id)) {
    // TODO(crbug.com/390138269): Once the kUserScriptUserExtensionToggle
    // feature is default enabled, we should make the
    // kDebuggerAPIRestrictedToDevMode feature control dev mode restriction
    // entirely and no longer be specific to the debugger API (while also
    // setting the debugger API to use dev mode in the features file so the dev
    // mode restriction is continued to be tested).

    // Restrict the debugger feature to dev mode if the extension feature is
    // enabled. But if the feature is disabled, then we treat it like any other
    // API.
    if (name() == "debugger" && !debugger_api_restricted) {
      return CreateAvailability(AvailabilityResult::kIsAvailable);
    }

    return CreateAvailability(AvailabilityResult::kRequiresDeveloperMode);
  }

  return CreateAvailability(AvailabilityResult::kIsAvailable);
}

Feature::Availability SimpleFeature::GetManifestAvailability(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    ManifestLocation location,
    int manifest_version) const {
  // Check extension type first to avoid granting platform app permissions
  // to component extensions.
  // HACK(kalman): user script -> extension. Solve this in a more generic way
  // when we compile feature files.
  Manifest::Type type_to_check =
      (type == Manifest::Type::kUserScript) ? Manifest::Type::kExtension : type;
  const auto allowed_extension_types = extension_types();
  if (!allowed_extension_types.empty() &&
      !std::ranges::contains(allowed_extension_types, type_to_check)) {
    return CreateAvailability(AvailabilityResult::kInvalidType, type);
  }

  if (!blocklist().empty() && IsIdInBlocklist(hashed_id)) {
    return CreateAvailability(AvailabilityResult::kFoundInBlocklist);
  }

  // TODO(benwells): don't grant all component extensions.
  // See http://crbug.com/41105605 for more details.
  // Component extensions can access any feature.
  // NOTE: Deliberately does not match EXTERNAL_COMPONENT.
  if (component_extensions_auto_granted() &&
      location == ManifestLocation::kComponent) {
    return CreateAvailability(AvailabilityResult::kIsAvailable);
  }

  if (!allowlist().empty() && !IsIdInAllowlist(hashed_id) &&
      !IsAllowlistedForTest(hashed_id)) {
    return CreateAvailability(AvailabilityResult::kNotFoundInAllowlist);
  }

  if (this->location() && !MatchesManifestLocation(location) &&
      !IsAllowlistedForTest(hashed_id)) {
    return CreateAvailability(AvailabilityResult::kInvalidLocation);
  }

  const auto min_version = min_manifest_version();
  if (min_version && manifest_version < *min_version) {
    return CreateAvailability(AvailabilityResult::kInvalidMinManifestVersion);
  }

  const auto max_version = max_manifest_version();
  if (max_version && manifest_version > *max_version) {
    return CreateAvailability(AvailabilityResult::kInvalidMaxManifestVersion);
  }

  return CreateAvailability(AvailabilityResult::kIsAvailable);
}

Feature::Availability SimpleFeature::GetContextAvailability(
    mojom::ContextType context,
    const GURL& url,
    bool is_for_service_worker) const {
  // TODO(lazyboy): This isn't quite right for Extension Service Worker
  // extension API calls, since there's no guarantee that the extension is
  // "active" in current renderer process when the API permission check is
  // done.
  const auto allowed_contexts = contexts();
  if (allowed_contexts && !std::ranges::contains(*allowed_contexts, context)) {
    return CreateAvailability(AvailabilityResult::kInvalidContext, context);
  }

  // TODO(kalman): Consider checking match patterns regardless of context
  // type.
  // Fewer surprises, and if the feature configuration wants to isolate
  // "matches" from say "privileged_extension" then they can use complex
  // features.
  const bool supports_url_matching =
      context == mojom::ContextType::kWebPage ||
      context == mojom::ContextType::kWebUi ||
      context == mojom::ContextType::kUntrustedWebUi;
  if (supports_url_matching && !MatchesURL(url)) {
    return CreateAvailability(AvailabilityResult::kInvalidUrl, url);
  }

  if (is_for_service_worker && disallow_for_service_workers()) {
    return CreateAvailability(AvailabilityResult::kInvalidContext);
  }

  return CreateAvailability(AvailabilityResult::kIsAvailable);
}

Feature::Availability SimpleFeature::RunDelegatedAvailabilityCheck(
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    Platform platform,
    int context_id,
    bool check_developer_mode,
    const ContextData& context_data) const {
  DCHECK(RequiresDelegatedAvailabilityCheck());
  DCHECK(HasDelegatedAvailabilityCheckHandler());
  if (!delegated_availability_check_handler_.Run(
          std::string(name()), extension, context, url, platform, context_id,
          check_developer_mode, context_data)) {
    return CreateAvailability(
        AvailabilityResult::kFailedDelegatedAvailabilityCheck);
  }
  return CreateAvailability(AvailabilityResult::kIsAvailable);
}

}  // namespace extensions
