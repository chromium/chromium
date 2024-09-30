// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/common/features/simple_feature.h"

#include <algorithm>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/content_features.h"
#include "extensions/common/extension_api.h"
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

using crx_file::id_util::HashedIdInHex;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

struct AllowlistInfo {
  AllowlistInfo() {
    const std::string& allowlisted_extension_id =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kAllowlistedExtensionID);
    hashed_id = HashedIdInHex(allowlisted_extension_id);
  }
  std::string hashed_id;
};
// A singleton copy of the --allowlisted-extension-id so that we don't need to
// copy it from the CommandLine each time.
base::LazyInstance<AllowlistInfo>::Leaky g_allowlist_info =
    LAZY_INSTANCE_INITIALIZER;

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
std::string GetDisplayName(Manifest::Type type) {
  switch (type) {
    case Manifest::TYPE_UNKNOWN:
      return "unknown";
    case Manifest::TYPE_EXTENSION:
      return "extension";
    case Manifest::TYPE_HOSTED_APP:
      return "hosted app";
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      return "legacy packaged app";
    case Manifest::TYPE_PLATFORM_APP:
      return "packaged app";
    case Manifest::TYPE_THEME:
      return "theme";
    case Manifest::TYPE_USER_SCRIPT:
      return "user script";
    case Manifest::TYPE_SHARED_MODULE:
      return "shared module";
    case Manifest::TYPE_LOGIN_SCREEN_EXTENSION:
      return "login screen extension";
    case Manifest::TYPE_CHROMEOS_SYSTEM_EXTENSION:
      return "chromeos system extension";
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

// Gets a human-readable name for the given context type, suitable for giving
// to developers in an error message.
std::string GetDisplayName(mojom::ContextType context) {
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
    case mojom::ContextType::kLockscreenExtension:
      return "lock screen app";
    case mojom::ContextType::kOffscreenExtension:
      return "offscreen document";
    case mojom::ContextType::kUserScript:
      return "user script";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

std::string GetDisplayName(mojom::FeatureSessionType session_type) {
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
template <typename EnumType>
std::string ListDisplayNames(const std::vector<EnumType>& enum_types) {
  std::string display_name_list;
  for (size_t i = 0; i < enum_types.size(); ++i) {
    // Pluralize type name.
    display_name_list += GetDisplayName(enum_types[i]) + "s";
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
                                const std::string& switch_name) {
  if (command_line->HasSwitch(switch_name + "=1"))
    return true;
  if (command_line->HasSwitch(std::string("enable-") + switch_name))
    return true;
  return false;
}

bool IsAllowlistedForTest(const HashedExtensionId& hashed_id) {
  const std::string& allowlisted_id = g_allowlist_info.Get().hashed_id;
  return !allowlisted_id.empty() && allowlisted_id == hashed_id.value();
}

}  // namespace

SimpleFeature::ScopedThreadUnsafeAllowlistForTest::
    ScopedThreadUnsafeAllowlistForTest(const std::string& id)
    : previous_id_(g_allowlist_info.Get().hashed_id) {
  g_allowlist_info.Get().hashed_id = HashedIdInHex(id);
}

SimpleFeature::ScopedThreadUnsafeAllowlistForTest::
    ~ScopedThreadUnsafeAllowlistForTest() {
  g_allowlist_info.Get().hashed_id = previous_id_;
}

SimpleFeature::SimpleFeature()
    : component_extensions_auto_granted_(true),
      is_internal_(false),
      disallow_for_service_workers_(false) {}

SimpleFeature::~SimpleFeature() = default;

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
                                            std::move(context_data))
            : CreateAvailability(MISSING_DELEGATED_AVAILABILITY_CHECK);

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

  bool is_for_service_worker = false;
  if (extension != nullptr && BackgroundInfo::IsServiceWorkerBased(extension) &&
      url.is_valid()) {
    const GURL script_url = extension->GetResourceURL(
        BackgroundInfo::GetBackgroundServiceWorkerScript(extension));
    if (script_url == url) {
      is_for_service_worker = true;
    }
  }

  Availability context_availability =
      GetContextAvailability(context, url, is_for_service_worker);
  if (!context_availability.is_available())
    return context_availability;

  // TODO(kalman): Assert that if the context was a webpage or WebUI context
  // then at some point a "matches" restriction was checked.

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
    case IS_AVAILABLE:
      return std::string();
    case NOT_FOUND_IN_ALLOWLIST:
    case FOUND_IN_BLOCKLIST:
      return base::StringPrintf(
          "'%s' is not allowed for specified extension ID.",
          name().c_str());
    case INVALID_URL:
      return base::StringPrintf("'%s' is not allowed on %s.",
                                name().c_str(), url.spec().c_str());
    case INVALID_TYPE:
      return base::StringPrintf(
          "'%s' is only allowed for %s, but this is a %s.",
          name().c_str(),
          ListDisplayNames(std::vector<Manifest::Type>(
              extension_types_.begin(), extension_types_.end())).c_str(),
          GetDisplayName(type).c_str());
    case INVALID_CONTEXT:
      DCHECK(contexts_);
      return base::StringPrintf(
          "'%s' is only allowed to run in %s, but this is a %s", name().c_str(),
          ListDisplayNames(std::vector<mojom::ContextType>(contexts_->begin(),
                                                           contexts_->end()))
              .c_str(),
          GetDisplayName(context).c_str());
    case INVALID_LOCATION:
      return base::StringPrintf(
          "'%s' is not allowed for specified install location.",
          name().c_str());
    case INVALID_PLATFORM:
      return base::StringPrintf(
          "'%s' is not allowed for specified platform.",
          name().c_str());
    case INVALID_MIN_MANIFEST_VERSION:
      DCHECK(min_manifest_version_);
      return base::StringPrintf(
          "'%s' requires manifest version of at least %d.", name().c_str(),
          *min_manifest_version_);
    case INVALID_MAX_MANIFEST_VERSION:
      DCHECK(max_manifest_version_);
      return base::StringPrintf(
          "'%s' requires manifest version of %d or lower.", name().c_str(),
          *max_manifest_version_);
    case INVALID_SESSION_TYPE:
      return base::StringPrintf(
          "'%s' is only allowed to run in %s sessions, but this is %s session.",
          name().c_str(),
          ListDisplayNames(std::vector<mojom::FeatureSessionType>(
                               session_types_.begin(), session_types_.end()))
              .c_str(),
          GetDisplayName(session_type).c_str());
    case NOT_PRESENT:
      return base::StringPrintf(
          "'%s' requires a different Feature that is not present.",
          name().c_str());
    case UNSUPPORTED_CHANNEL:
      return base::StringPrintf(
          "'%s' requires %s channel or newer, but this is the %s channel.",
          name().c_str(), version_info::GetChannelString(channel).data(),
          version_info::GetChannelString(GetCurrentChannel()).data());
    case MISSING_COMMAND_LINE_SWITCH:
      DCHECK(command_line_switch_);
      return base::StringPrintf(
          "'%s' requires the '%s' command line switch to be enabled.",
          name().c_str(), command_line_switch_->c_str());
    case FEATURE_FLAG_DISABLED:
      DCHECK(feature_flag_);
      return base::StringPrintf(
          "'%s' requires the '%s' feature flag to be enabled.", name().c_str(),
          feature_flag_->c_str());
    case REQUIRES_DEVELOPER_MODE:
      return base::StringPrintf(
          "'%s' requires the user to have developer mode enabled.",
          name().c_str());
    case MISSING_DELEGATED_AVAILABILITY_CHECK:
      return base::StringPrintf(
          "'%s' is missing its delegated availability check", name().c_str());
    case FAILED_DELEGATED_AVAILABILITY_CHECK:
      return base::StringPrintf("'%s' failed its delegated availability check.",
                                name().c_str());
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
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
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, url,
                                     mojom::ContextType::kUnspecified,
                                     version_info::Channel::UNKNOWN,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    mojom::ContextType context) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     context, version_info::Channel::UNKNOWN,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    version_info::Channel channel) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     mojom::ContextType::kUnspecified, channel,
                                     mojom::FeatureSessionType::kUnknown));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    mojom::FeatureSessionType session_type) const {
  return Availability(
      result,
      GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                             mojom::ContextType::kUnspecified,
                             version_info::Channel::UNKNOWN, session_type));
}

bool SimpleFeature::IsInternal() const {
  return is_internal_;
}

bool SimpleFeature::IsIdInBlocklist(const HashedExtensionId& hashed_id) const {
  return IsIdInList(hashed_id, blocklist_);
}

bool SimpleFeature::IsIdInAllowlist(const HashedExtensionId& hashed_id) const {
  return IsIdInList(hashed_id, allowlist_);
}

// static
bool SimpleFeature::IsIdInArray(const ExtensionId& extension_id,
                                const char* const array[],
                                size_t array_length) {
  if (!IsValidExtensionId(extension_id))
    return false;

  const char* const* start = array;
  const char* const* end = array + array_length;

  return ((std::find(start, end, extension_id) != end) ||
          (std::find(start, end, HashedIdInHex(extension_id)) != end));
}

// static
bool SimpleFeature::IsIdInList(const HashedExtensionId& hashed_id,
                               const std::vector<std::string>& list) {
  if (!IsValidHashedExtensionId(hashed_id))
    return false;

  return base::Contains(list, hashed_id.value());
}

bool SimpleFeature::MatchesManifestLocation(
    ManifestLocation manifest_location) const {
  DCHECK(location_);
  switch (*location_) {
    case SimpleFeature::COMPONENT_LOCATION:
      return manifest_location == ManifestLocation::kComponent;
    case SimpleFeature::EXTERNAL_COMPONENT_LOCATION:
      return manifest_location == ManifestLocation::kExternalComponent;
    case SimpleFeature::POLICY_LOCATION:
      return manifest_location == ManifestLocation::kExternalPolicy ||
             manifest_location == ManifestLocation::kExternalPolicyDownload;
    case SimpleFeature::UNPACKED_LOCATION:
      return Manifest::IsUnpackedLocation(manifest_location);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool SimpleFeature::MatchesSessionTypes(
    mojom::FeatureSessionType session_type) const {
  if (session_types_.empty())
    return true;

  if (base::Contains(session_types_, session_type))
    return true;

  // AUTOLAUNCHED_KIOSK session type is subset of KIOSK - accept auto-lauched
  // kiosk session if kiosk session is allowed. This is the only exception to
  // rejecting session type that is not present in |session_types_|
  return session_type == mojom::FeatureSessionType::kAutolaunchedKiosk &&
         base::Contains(session_types_, mojom::FeatureSessionType::kKiosk);
}

bool SimpleFeature::RequiresDelegatedAvailabilityCheck() const {
  return requires_delegated_availability_check_;
}

bool SimpleFeature::HasDelegatedAvailabilityCheckHandler() const {
  return !delegated_availability_check_handler_.is_null();
}

void SimpleFeature::SetDelegatedAvailabilityCheckHandler(
    DelegatedAvailabilityCheckHandler handler) {
  DCHECK(RequiresDelegatedAvailabilityCheck());
  DCHECK(!HasDelegatedAvailabilityCheckHandler());
  delegated_availability_check_handler_ = handler;
}

Feature::Availability SimpleFeature::CheckDependencies(
    const base::RepeatingCallback<Availability(const Feature*)>& checker)
    const {
  for (const auto& dep_name : dependencies_) {
    const Feature* dependency =
        ExtensionAPI::GetSharedInstance()->GetFeatureDependency(dep_name);
    if (!dependency)
      return CreateAvailability(NOT_PRESENT);
    Availability dependency_availability = checker.Run(dependency);
    if (!dependency_availability.is_available())
      return dependency_availability;
  }
  return CreateAvailability(IS_AVAILABLE);
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
  // As above, just the bare-bones check.
  return hashed_id.value().length() == 40;
}

void SimpleFeature::set_blocklist(
    std::initializer_list<const char* const> blocklist) {
  blocklist_.assign(blocklist.begin(), blocklist.end());
}

void SimpleFeature::set_command_line_switch(
    std::string_view command_line_switch) {
  command_line_switch_ = std::string(command_line_switch);
}

void SimpleFeature::set_contexts(
    std::initializer_list<mojom::ContextType> contexts) {
  contexts_ = contexts;
}

void SimpleFeature::set_dependencies(
    std::initializer_list<const char* const> dependencies) {
  dependencies_.assign(dependencies.begin(), dependencies.end());
}

void SimpleFeature::set_extension_types(
    std::initializer_list<Manifest::Type> types) {
  extension_types_ = types;
}

void SimpleFeature::set_feature_flag(std::string_view feature_flag) {
  feature_flag_ = std::string(feature_flag);
}

void SimpleFeature::set_session_types(
    std::initializer_list<mojom::FeatureSessionType> types) {
  session_types_ = types;
}

void SimpleFeature::set_matches(
    std::initializer_list<const char* const> matches) {
  matches_.ClearPatterns();
  for (const auto* pattern : matches)
    matches_.AddPattern(URLPattern(URLPattern::SCHEME_ALL, pattern));
}

void SimpleFeature::set_platforms(std::initializer_list<Platform> platforms) {
  platforms_ = platforms;
}

void SimpleFeature::set_allowlist(
    std::initializer_list<const char* const> allowlist) {
  allowlist_.assign(allowlist.begin(), allowlist.end());
}

Feature::Availability SimpleFeature::GetEnvironmentAvailability(
    Platform platform,
    version_info::Channel channel,
    mojom::FeatureSessionType session_type,
    int context_id,
    bool check_developer_mode) const {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!platforms_.empty() && !base::Contains(platforms_, platform))
    return CreateAvailability(INVALID_PLATFORM);

  if (channel_ && *channel_ < GetCurrentChannel()) {
    // If the user has the kEnableExperimentalExtensionApis commandline flag
    // appended, we ignore channel restrictions.
    if (!ignore_channel_) {
      ignore_channel_ =
          command_line->HasSwitch(switches::kEnableExperimentalExtensionApis);
    }
    if (!(*ignore_channel_))
      return CreateAvailability(UNSUPPORTED_CHANNEL, *channel_);
  }

  if (command_line_switch_ &&
      !IsCommandLineSwitchEnabled(command_line, *command_line_switch_)) {
    return CreateAvailability(MISSING_COMMAND_LINE_SWITCH);
  }

  if (feature_flag_ && !IsFeatureFlagEnabled(*feature_flag_))
    return CreateAvailability(FEATURE_FLAG_DISABLED);

  if (!MatchesSessionTypes(session_type))
    return CreateAvailability(INVALID_SESSION_TYPE, session_type);

  if (check_developer_mode &&
      developer_mode_only_ && !GetCurrentDeveloperMode(context_id)) {
    return CreateAvailability(REQUIRES_DEVELOPER_MODE);
  }

  return CreateAvailability(IS_AVAILABLE);
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
      (type == Manifest::TYPE_USER_SCRIPT) ? Manifest::TYPE_EXTENSION : type;
  if (!extension_types_.empty() &&
      !base::Contains(extension_types_, type_to_check)) {
    return CreateAvailability(INVALID_TYPE, type);
  }

  if (!blocklist_.empty() && IsIdInBlocklist(hashed_id))
    return CreateAvailability(FOUND_IN_BLOCKLIST);

  // TODO(benwells): don't grant all component extensions.
  // See http://crbug.com/370375 for more details.
  // Component extensions can access any feature.
  // NOTE: Deliberately does not match EXTERNAL_COMPONENT.
  if (component_extensions_auto_granted_ &&
      location == ManifestLocation::kComponent)
    return CreateAvailability(IS_AVAILABLE);

  if (!allowlist_.empty() && !IsIdInAllowlist(hashed_id) &&
      !IsAllowlistedForTest(hashed_id)) {
    return CreateAvailability(NOT_FOUND_IN_ALLOWLIST);
  }

  if (location_ && !MatchesManifestLocation(location) &&
      !IsAllowlistedForTest(hashed_id)) {
    return CreateAvailability(INVALID_LOCATION);
  }

  if (min_manifest_version_ && manifest_version < *min_manifest_version_)
    return CreateAvailability(INVALID_MIN_MANIFEST_VERSION);

  if (max_manifest_version_ && manifest_version > *max_manifest_version_)
    return CreateAvailability(INVALID_MAX_MANIFEST_VERSION);

  return CreateAvailability(IS_AVAILABLE);
}

Feature::Availability SimpleFeature::GetContextAvailability(
    mojom::ContextType context,
    const GURL& url,
    bool is_for_service_worker) const {
  // TODO(lazyboy): This isn't quite right for Extension Service Worker
  // extension API calls, since there's no guarantee that the extension is
  // "active" in current renderer process when the API permission check is
  // done.
  if (contexts_ && !base::Contains(*contexts_, context))
    return CreateAvailability(INVALID_CONTEXT, context);

  // TODO(kalman): Consider checking |matches_| regardless of context type.
  // Fewer surprises, and if the feature configuration wants to isolate
  // "matches" from say "privileged_extension" then they can use complex
  // features.
  const bool supports_url_matching =
      context == mojom::ContextType::kWebPage ||
      context == mojom::ContextType::kWebUi ||
      context == mojom::ContextType::kUntrustedWebUi;
  if (supports_url_matching && !matches_.MatchesURL(url)) {
    return CreateAvailability(INVALID_URL, url);
  }

  if (is_for_service_worker && disallow_for_service_workers_)
    return CreateAvailability(INVALID_CONTEXT);

  return CreateAvailability(IS_AVAILABLE);
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
          name_, extension, context, url, platform, context_id,
          check_developer_mode, context_data)) {
    return CreateAvailability(FAILED_DELEGATED_AVAILABILITY_CHECK);
  }
  return CreateAvailability(IS_AVAILABLE);
}

}  // namespace extensions
