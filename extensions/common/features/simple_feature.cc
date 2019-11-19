// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/features/simple_feature.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/switches.h"

using crx_file::id_util::HashedIdInHex;

namespace extensions {

namespace {

struct AllowlistInfo {
  AllowlistInfo()
      : hashed_id(HashedIdInHex(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                switches::kWhitelistedExtensionID))) {}
  std::string hashed_id;
};
// A singleton copy of the --whitelisted-extension-id so that we don't need to
// copy it from the CommandLine each time.
base::LazyInstance<AllowlistInfo>::Leaky g_allowlist_info =
    LAZY_INSTANCE_INITIALIZER;

Feature::Availability IsAvailableToManifestForBind(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Feature::Platform platform,
    const Feature* feature) {
  return feature->IsAvailableToManifest(hashed_id, type, location,
                                        manifest_version, platform);
}

Feature::Availability IsAvailableToContextForBind(const Extension* extension,
                                                  Feature::Context context,
                                                  const GURL& url,
                                                  Feature::Platform platform,
                                                  const Feature* feature) {
  return feature->IsAvailableToContext(extension, context, url, platform);
}

Feature::Availability IsAvailableToEnvironmentForBind(const Feature* feature) {
  return feature->IsAvailableToEnvironment();
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
    case Manifest::NUM_LOAD_TYPES:
      NOTREACHED();
  }
  NOTREACHED();
  return "";
}

// Gets a human-readable name for the given context type, suitable for giving
// to developers in an error message.
std::string GetDisplayName(Feature::Context context) {
  switch (context) {
    case Feature::UNSPECIFIED_CONTEXT:
      return "unknown";
    case Feature::BLESSED_EXTENSION_CONTEXT:
      // "privileged" is vague but hopefully the developer will understand that
      // means background or app window.
      return "privileged page";
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      // "iframe" is a bit of a lie/oversimplification, but that's the most
      // common unblessed context.
      return "extension iframe";
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return "content script";
    case Feature::WEB_PAGE_CONTEXT:
      return "web page";
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return "hosted app";
    case Feature::WEBUI_CONTEXT:
      return "webui";
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      return "lock screen app";
  }
  NOTREACHED();
  return "";
}

std::string GetDisplayName(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return "trunk";
    case version_info::Channel::CANARY:
      return "canary";
    case version_info::Channel::DEV:
      return "dev";
    case version_info::Channel::BETA:
      return "beta";
    case version_info::Channel::STABLE:
      return "stable";
  }
  NOTREACHED();
  return "";
}

std::string GetDisplayName(FeatureSessionType session_type) {
  switch (session_type) {
    case FeatureSessionType::INITIAL:
      return "user-less";
    case FeatureSessionType::UNKNOWN:
      return "unknown";
    case FeatureSessionType::KIOSK:
      return "kiosk app";
    case FeatureSessionType::AUTOLAUNCHED_KIOSK:
      return "auto-launched kiosk app";
    case FeatureSessionType::REGULAR:
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

SimpleFeature::~SimpleFeature() {}

Feature::Availability SimpleFeature::IsAvailableToManifest(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    Manifest::Location location,
    int manifest_version,
    Platform platform) const {
  Availability environment_availability = GetEnvironmentAvailability(
      platform, GetCurrentChannel(), GetCurrentFeatureSessionType());
  if (!environment_availability.is_available())
    return environment_availability;
  Availability manifest_availability =
      GetManifestAvailability(hashed_id, type, location, manifest_version);
  if (!manifest_availability.is_available())
    return manifest_availability;

  return CheckDependencies(base::Bind(&IsAvailableToManifestForBind, hashed_id,
                                      type, location, manifest_version,
                                      platform));
}

Feature::Availability SimpleFeature::IsAvailableToContext(
    const Extension* extension,
    Feature::Context context,
    const GURL& url,
    Platform platform) const {
  Availability environment_availability = GetEnvironmentAvailability(
      platform, GetCurrentChannel(), GetCurrentFeatureSessionType());
  if (!environment_availability.is_available())
    return environment_availability;

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
  return CheckDependencies(base::Bind(&IsAvailableToContextForBind,
                                      base::RetainedRef(extension), context,
                                      url, platform));
}

Feature::Availability SimpleFeature::IsAvailableToEnvironment() const {
  Availability environment_availability =
      GetEnvironmentAvailability(GetCurrentPlatform(), GetCurrentChannel(),
                                 GetCurrentFeatureSessionType());
  if (!environment_availability.is_available())
    return environment_availability;
  return CheckDependencies(base::Bind(&IsAvailableToEnvironmentForBind));
}

std::string SimpleFeature::GetAvailabilityMessage(
    AvailabilityResult result,
    Manifest::Type type,
    const GURL& url,
    Context context,
    version_info::Channel channel,
    FeatureSessionType session_type) const {
  switch (result) {
    case IS_AVAILABLE:
      return std::string();
    case NOT_FOUND_IN_WHITELIST:
    case FOUND_IN_BLACKLIST:
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
      return base::StringPrintf(
          "'%s' is only allowed to run in %s, but this is a %s",
          name().c_str(),
          ListDisplayNames(std::vector<Context>(
              contexts_.begin(), contexts_.end())).c_str(),
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
          ListDisplayNames(std::vector<FeatureSessionType>(
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
          name().c_str(), GetDisplayName(channel).c_str(),
          GetDisplayName(GetCurrentChannel()).c_str());
    case MISSING_COMMAND_LINE_SWITCH:
      DCHECK(command_line_switch_);
      return base::StringPrintf(
          "'%s' requires the '%s' command line switch to be enabled.",
          name().c_str(), command_line_switch_->c_str());
  }

  NOTREACHED();
  return std::string();
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result) const {
  return Availability(
      result, GetAvailabilityMessage(
                  result, Manifest::TYPE_UNKNOWN, GURL(), UNSPECIFIED_CONTEXT,
                  version_info::Channel::UNKNOWN, FeatureSessionType::UNKNOWN));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result, Manifest::Type type) const {
  return Availability(
      result, GetAvailabilityMessage(result, type, GURL(), UNSPECIFIED_CONTEXT,
                                     version_info::Channel::UNKNOWN,
                                     FeatureSessionType::UNKNOWN));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    const GURL& url) const {
  return Availability(
      result, GetAvailabilityMessage(
                  result, Manifest::TYPE_UNKNOWN, url, UNSPECIFIED_CONTEXT,
                  version_info::Channel::UNKNOWN, FeatureSessionType::UNKNOWN));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    Context context) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     context, version_info::Channel::UNKNOWN,
                                     FeatureSessionType::UNKNOWN));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    version_info::Channel channel) const {
  return Availability(
      result, GetAvailabilityMessage(result, Manifest::TYPE_UNKNOWN, GURL(),
                                     UNSPECIFIED_CONTEXT, channel,
                                     FeatureSessionType::UNKNOWN));
}

Feature::Availability SimpleFeature::CreateAvailability(
    AvailabilityResult result,
    FeatureSessionType session_type) const {
  return Availability(
      result, GetAvailabilityMessage(
                  result, Manifest::TYPE_UNKNOWN, GURL(), UNSPECIFIED_CONTEXT,
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
bool SimpleFeature::IsIdInArray(const std::string& extension_id,
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
    Manifest::Location manifest_location) const {
  DCHECK(location_);
  switch (*location_) {
    case SimpleFeature::COMPONENT_LOCATION:
      return manifest_location == Manifest::COMPONENT;
    case SimpleFeature::EXTERNAL_COMPONENT_LOCATION:
      return manifest_location == Manifest::EXTERNAL_COMPONENT;
    case SimpleFeature::POLICY_LOCATION:
      return manifest_location == Manifest::EXTERNAL_POLICY ||
             manifest_location == Manifest::EXTERNAL_POLICY_DOWNLOAD;
    case SimpleFeature::UNPACKED_LOCATION:
      return Manifest::IsUnpackedLocation(manifest_location);
  }
  NOTREACHED();
  return false;
}

bool SimpleFeature::MatchesSessionTypes(FeatureSessionType session_type) const {
  if (session_types_.empty())
    return true;

  if (base::Contains(session_types_, session_type))
    return true;

  // AUTOLAUNCHED_KIOSK session type is subset of KIOSK - accept auto-lauched
  // kiosk session if kiosk session is allowed. This is the only exception to
  // rejecting session type that is not present in |session_types_|
  return session_type == FeatureSessionType::AUTOLAUNCHED_KIOSK &&
         base::Contains(session_types_, FeatureSessionType::KIOSK);
}

Feature::Availability SimpleFeature::CheckDependencies(
    const base::Callback<Availability(const Feature*)>& checker) const {
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
bool SimpleFeature::IsValidExtensionId(const std::string& extension_id) {
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
    base::StringPiece command_line_switch) {
  command_line_switch_ = command_line_switch.as_string();
}

void SimpleFeature::set_contexts(std::initializer_list<Context> contexts) {
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

void SimpleFeature::set_session_types(
    std::initializer_list<FeatureSessionType> types) {
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
    FeatureSessionType session_type) const {
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

  if (!MatchesSessionTypes(session_type))
    return CreateAvailability(INVALID_SESSION_TYPE, session_type);

  return CreateAvailability(IS_AVAILABLE);
}

Feature::Availability SimpleFeature::GetManifestAvailability(
    const HashedExtensionId& hashed_id,
    Manifest::Type type,
    Manifest::Location location,
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
    return CreateAvailability(FOUND_IN_BLACKLIST);

  // TODO(benwells): don't grant all component extensions.
  // See http://crbug.com/370375 for more details.
  // Component extensions can access any feature.
  // NOTE: Deliberately does not match EXTERNAL_COMPONENT.
  if (component_extensions_auto_granted_ && location == Manifest::COMPONENT)
    return CreateAvailability(IS_AVAILABLE);

  if (!allowlist_.empty() && !IsIdInAllowlist(hashed_id) &&
      !IsAllowlistedForTest(hashed_id)) {
    return CreateAvailability(NOT_FOUND_IN_WHITELIST);
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
    Feature::Context context,
    const GURL& url,
    bool is_for_service_worker) const {
  // TODO(lazyboy): This isn't quite right for Extension Service Worker
  // extension API calls, since there's no guarantee that the extension is
  // "active" in current renderer process when the API permission check is
  // done.
  if (!contexts_.empty() && !base::Contains(contexts_, context))
    return CreateAvailability(INVALID_CONTEXT, context);

  // TODO(kalman): Consider checking |matches_| regardless of context type.
  // Fewer surprises, and if the feature configuration wants to isolate
  // "matches" from say "blessed_extension" then they can use complex features.
  if ((context == WEB_PAGE_CONTEXT || context == WEBUI_CONTEXT) &&
      !matches_.MatchesURL(url)) {
    return CreateAvailability(INVALID_URL, url);
  }

  if (is_for_service_worker && disallow_for_service_workers_)
    return CreateAvailability(INVALID_CONTEXT);

  return CreateAvailability(IS_AVAILABLE);
}

}  // namespace extensions
