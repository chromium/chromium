// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_CONSTANTS_H_
#define EXTENSIONS_COMMON_MANIFEST_CONSTANTS_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace extensions {

// Keys used in JSON representation of extensions.
namespace manifest_keys {

// A list of keys that do not generate warnings when specified in the manifest,
// despite the fact that they are not recognized by Chrome. Keys should be
// added here if they are widely adopted but a developer is unlikely to expect
// that it would do anything in Chrome, and so wouldn't benefit from a warning.
inline constexpr const char* const kIgnoredUnrecognizedKeys[] = {
    // This is used by non-Chromium browsers:
    // https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_specific_settings
    "browser_specific_settings",
    // This is part of the JSON schema definition:
    // https://json-schema.org/understanding-json-schema/reference/schema#schema
    "$schema"};

inline constexpr char kAboutPage[] = "about_page";
inline constexpr char kAction[] = "action";
inline constexpr char kActionDefaultIcon[] = "default_icon";
inline constexpr char kActionDefaultPopup[] = "default_popup";
inline constexpr char kActionDefaultState[] = "default_state";
inline constexpr char kActionDefaultTitle[] = "default_title";
inline constexpr char kApp[] = "app";
inline constexpr char kAutomation[] = "automation";
inline constexpr char kBackground[] = "background";
inline constexpr char kBackgroundAllowJsAccess[] = "background.allow_js_access";
inline constexpr char kBackgroundPage[] = "background.page";
inline constexpr char kBackgroundPersistent[] = "background.persistent";
inline constexpr char kBackgroundScripts[] = "background.scripts";
inline constexpr char kBackgroundServiceWorkerScript[] =
    "background.service_worker";
inline constexpr char kBackgroundServiceWorkerType[] = "background.type";
inline constexpr char kBluetooth[] = "bluetooth";
inline constexpr char kBookmarkUI[] = "bookmarks_ui";
inline constexpr char kBrowserAction[] = "browser_action";
inline constexpr char kChromeOSSystemExtension[] = "chromeos_system_extension";
inline constexpr char kCommands[] = "commands";
inline constexpr char kContentCapabilities[] = "content_capabilities";
inline constexpr char kContentSecurityPolicy[] = "content_security_policy";
inline constexpr char kContentSecurityPolicy_ExtensionPagesPath[] =
    "content_security_policy.extension_pages";
inline constexpr char kContentSecurityPolicy_SandboxedPagesPath[] =
    "content_security_policy.sandbox";
inline constexpr char kConvertedFromUserScript[] = "converted_from_user_script";
inline constexpr char kCurrentLocale[] = "current_locale";
inline constexpr char kDefaultLocale[] = "default_locale";
inline constexpr char kDescription[] = "description";
inline constexpr char kDevToolsPage[] = "devtools_page";
inline constexpr char kDifferentialFingerprint[] = "differential_fingerprint";
inline constexpr char kDisplayInLauncher[] = "display_in_launcher";
inline constexpr char kDisplayInNewTabPage[] = "display_in_new_tab_page";
inline constexpr char kEventName[] = "event_name";
inline constexpr char kExternallyConnectable[] = "externally_connectable";
inline constexpr char kEventRules[] = "event_rules";
inline constexpr char kFileAccessList[] = "file_access";
inline constexpr char kFileBrowserHandlerId[] = "id";
inline constexpr char kFileBrowserHandlers[] = "file_browser_handlers";
inline constexpr char kFileFilters[] = "file_filters";
inline constexpr char kFileHandlerExtensions[] = "extensions";
inline constexpr char kFileHandlerIncludeDirectories[] = "include_directories";
inline constexpr char kFileHandlerTypes[] = "types";
inline constexpr char kFileHandlerVerb[] = "verb";
inline constexpr char kFileHandlers[] = "file_handlers";
inline constexpr char kGlobal[] = "global";
inline constexpr char kHandwritingLanguage[] = "handwriting_language";
inline constexpr char kHideBookmarkButton[] = "hide_bookmark_button";
inline constexpr char kHomepageURL[] = "homepage_url";
inline constexpr char kHostPermissions[] = "host_permissions";
inline constexpr char kIcons[] = "icons";
inline constexpr char kIconVariants[] = "icon_variants";
inline constexpr char kId[] = "id";
inline constexpr char kImeOptionsPage[] = "options_page";
inline constexpr char kIndicator[] = "indicator";
inline constexpr char kInputComponents[] = "input_components";
inline constexpr char kInputView[] = "input_view";
inline constexpr char kKey[] = "key";
inline constexpr char kKiosk[] = "kiosk";
inline constexpr char kKioskAlwaysUpdate[] = "kiosk.always_update";
inline constexpr char kKioskEnabled[] = "kiosk_enabled";
inline constexpr char kKioskOnly[] = "kiosk_only";
inline constexpr char kKioskMode[] = "kiosk_mode";
inline constexpr char kKioskRequiredPlatformVersion[] =
    "kiosk.required_platform_version";
inline constexpr char kKioskSecondaryApps[] = "kiosk_secondary_apps";
inline constexpr char kLanguage[] = "language";
inline constexpr char kLaunch[] = "app.launch";
inline constexpr char kLaunchContainer[] = "app.launch.container";
inline constexpr char kLaunchHeight[] = "app.launch.height";
inline constexpr char kLaunchLocalPath[] = "app.launch.local_path";
inline constexpr char kLaunchWebURL[] = "app.launch.web_url";
inline constexpr char kLaunchWidth[] = "app.launch.width";
inline constexpr char kLayouts[] = "layouts";
inline constexpr char kLinkedAppIcons[] = "app.linked_icons";
inline constexpr char kLinkedAppIconURL[] = "url";
inline constexpr char kLinkedAppIconSize[] = "size";
inline constexpr char kManifestVersion[] = "manifest_version";
inline constexpr char kMatches[] = "matches";
inline constexpr char kMIMETypes[] = "mime_types";
inline constexpr char kMimeTypesHandler[] = "mime_types_handler";
inline constexpr char kMinimumChromeVersion[] = "minimum_chrome_version";
inline constexpr char kNaClModules[] = "nacl_modules";
inline constexpr char kNaClModulesMIMEType[] = "mime_type";
inline constexpr char kNaClModulesPath[] = "path";
inline constexpr char kName[] = "name";
inline constexpr char kNativelyConnectable[] = "natively_connectable";
inline constexpr char kOfflineEnabled[] = "offline_enabled";
inline constexpr char kOmniboxKeyword[] = "omnibox.keyword";
inline constexpr char kOptionalHostPermissions[] = "optional_host_permissions";
inline constexpr char kOptionalPermissions[] = "optional_permissions";
inline constexpr char kOptionsPage[] = "options_page";
inline constexpr char kOptionsUI[] = "options_ui";
inline constexpr char kOverrideHomepage[] =
    "chrome_settings_overrides.homepage";
inline constexpr char kOverrideSearchProvider[] =
    "chrome_settings_overrides.search_provider";
inline constexpr char kOverrideStartupPage[] =
    "chrome_settings_overrides.startup_pages";
inline constexpr char kPageAction[] = "page_action";
inline constexpr char kPermissions[] = "permissions";
inline constexpr char kPlatformAppBackground[] = "app.background";
inline constexpr char kPlatformAppBackgroundPage[] = "app.background.page";
inline constexpr char kPlatformAppBackgroundScripts[] =
    "app.background.scripts";
inline constexpr char kPlatformAppContentSecurityPolicy[] =
    "app.content_security_policy";
inline constexpr char kPublicKey[] = "key";
inline constexpr char kRemoveButton[] = "remove_button";
inline constexpr char kReplacementWebApp[] = "replacement_web_app";
inline constexpr char kSandboxedPages[] = "sandbox.pages";
inline constexpr char kSandboxedPagesCSP[] = "sandbox.content_security_policy";
inline constexpr char kSettingsOverride[] = "chrome_settings_overrides";
inline constexpr char kSettingsOverrideAlternateUrls[] =
    "chrome_settings_overrides.search_provider.alternate_urls";
inline constexpr char kShortName[] = "short_name";
inline constexpr char kSockets[] = "sockets";
inline constexpr char kStorageManagedSchema[] = "storage.managed_schema";
inline constexpr char kSuggestedKey[] = "suggested_key";
inline constexpr char kSystemIndicator[] = "system_indicator";
inline constexpr char kTheme[] = "theme";
inline constexpr char kThemeColors[] = "colors";
inline constexpr char kThemeDisplayProperties[] = "properties";
inline constexpr char kThemeImages[] = "images";
inline constexpr char kThemeTints[] = "tints";
inline constexpr char kTrialTokens[] = "trial_tokens";
inline constexpr char kTtsEngine[] = "tts_engine";
inline constexpr char kTtsEngineSampleRate[] = "sample_rate";
inline constexpr char kTtsEngineBufferSize[] = "buffer_size";
inline constexpr char kTtsVoices[] = "voices";
inline constexpr char kTtsVoicesEventTypeEnd[] = "end";
inline constexpr char kTtsVoicesEventTypeError[] = "error";
inline constexpr char kTtsVoicesEventTypeMarker[] = "marker";
inline constexpr char kTtsVoicesEventTypeSentence[] = "sentence";
inline constexpr char kTtsVoicesEventTypeStart[] = "start";
inline constexpr char kTtsVoicesEventTypeWord[] = "word";
inline constexpr char kTtsVoicesEventTypes[] = "event_types";
inline constexpr char kTtsVoicesLang[] = "lang";
inline constexpr char kTtsVoicesRemote[] = "remote";
inline constexpr char kTtsVoicesVoiceName[] = "voice_name";
inline constexpr char kUpdateURL[] = "update_url";
inline constexpr char kUrlHandlers[] = "url_handlers";
inline constexpr char kUrlHandlerTitle[] = "title";
inline constexpr char kUsbPrinters[] = "usb_printers";
inline constexpr char kVersion[] = "version";
inline constexpr char kVersionName[] = "version_name";
inline constexpr char kWebURLs[] = "app.urls";
inline constexpr char kWebview[] = "webview";
inline constexpr char kWebviewAccessibleResources[] = "accessible_resources";
inline constexpr char kWebviewName[] = "name";
inline constexpr char kWebviewPartitions[] = "partitions";
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr char kFileSystemProviderCapabilities[] =
    "file_system_provider_capabilities";
inline constexpr char kActionHandlers[] = "action_handlers";
inline constexpr char kActionHandlerActionKey[] = "action";
inline constexpr char kActionHandlerEnabledOnLockScreenKey[] =
    "enabled_on_lock_screen";
#endif

}  // namespace manifest_keys

// Some values expected in manifests.
namespace manifest_values {

inline constexpr char kActionCommandEvent[] = "_execute_action";
inline constexpr char kApiKey[] = "api_key";
inline constexpr char kBrowserActionCommandEvent[] = "_execute_browser_action";
inline constexpr char kIncognitoNotAllowed[] = "not_allowed";
inline constexpr char kIncognitoSplit[] = "split";
inline constexpr char kIncognitoSpanning[] = "spanning";
inline constexpr char kKeybindingPlatformChromeOs[] = "chromeos";
inline constexpr char kKeybindingPlatformDefault[] = "default";
inline constexpr char kKeybindingPlatformLinux[] = "linux";
inline constexpr char kKeybindingPlatformMac[] = "mac";
inline constexpr char kKeybindingPlatformWin[] = "windows";
inline constexpr char kKeyAlt[] = "Alt";
inline constexpr char kKeyComma[] = "Comma";
inline constexpr char kKeyCommand[] = "Command";
inline constexpr char kKeyCtrl[] = "Ctrl";
inline constexpr char kKeyDel[] = "Delete";
inline constexpr char kKeyDown[] = "Down";
inline constexpr char kKeyEnd[] = "End";
inline constexpr char kKeyHome[] = "Home";
inline constexpr char kKeyIns[] = "Insert";
inline constexpr char kKeyLeft[] = "Left";
inline constexpr char kKeyMacCtrl[] = "MacCtrl";
inline constexpr char kKeyMediaNextTrack[] = "MediaNextTrack";
inline constexpr char kKeyMediaPlayPause[] = "MediaPlayPause";
inline constexpr char kKeyMediaPrevTrack[] = "MediaPrevTrack";
inline constexpr char kKeyMediaStop[] = "MediaStop";
inline constexpr char kKeyPgDwn[] = "PageDown";
inline constexpr char kKeyPgUp[] = "PageUp";
inline constexpr char kKeyPeriod[] = "Period";
inline constexpr char kKeyRight[] = "Right";
inline constexpr char kKeySearch[] = "Search";
inline constexpr char kKeySeparator[] = "+";
inline constexpr char kKeyShift[] = "Shift";
inline constexpr char kKeySpace[] = "Space";
inline constexpr char kKeyTab[] = "Tab";
inline constexpr char kKeyUp[] = "Up";
inline constexpr char kLaunchContainerPanelDeprecated[] = "panel";
inline constexpr char kLaunchContainerTab[] = "tab";
inline constexpr char kLaunchContainerWindow[] = "window";
inline constexpr char kPageActionCommandEvent[] = "_execute_page_action";

}  // namespace manifest_values

// Extension-related error messages. Some of these are simple patterns, where a
// '*' is replaced at runtime with a specific value. This is used instead of
// printf because we want to unit test them and scanf is hard to make
// cross-platform.
namespace manifest_errors {

inline constexpr char kActiveTabPermissionNotGranted[] =
    "The 'activeTab' permission is not in effect because this extension has "
    "not been in invoked.";
inline constexpr char kAllURLOrActiveTabNeeded[] =
    "Either the '<all_urls>' or 'activeTab' permission is required.";
inline constexpr char kAppsNotEnabled[] = "Apps are not enabled.";
inline constexpr char16_t kBackgroundPermissionNeeded[] =
    u"Hosted apps that use 'background_page' must have the 'background' "
    "permission.";
inline constexpr char16_t kBackgroundRequiredForPlatformApps[] =
    u"Packaged apps must have a background page or background scripts.";
inline constexpr char kCannotAccessAboutUrl[] =
    "Cannot access \"*\" at origin \"*\". Extension must have permission to "
    "access the frame's origin, and matchAboutBlank must be true.";
inline constexpr char kCannotAccessChromeUrl[] =
    "Cannot access a chrome:// URL";
inline constexpr char kCannotAccessExtensionUrl[] =
    "Cannot access a chrome-extension:// URL of different extension";
// This deliberately does not contain a URL. Otherwise an extension can parse
// error messages and determine the URLs of open tabs without having appropriate
// permissions to see these URLs.
inline constexpr char kCannotAccessPage[] =
    "Cannot access contents of the page. "
    "Extension manifest must request permission to access the respective host.";
// Use this error message with caution and only if the extension triggering it
// has tabs permission. Otherwise, URLs may be leaked to extensions.
inline constexpr char kCannotAccessPageWithUrl[] =
    "Cannot access contents of url \"*\". "
    "Extension manifest must request permission to access this host.";
inline constexpr char kCannotChangeExtensionID[] =
    "Installed extensions cannot change their IDs.";
inline constexpr char kCannotClaimAllHostsInExtent[] =
    "Cannot claim all hosts ('*') in an extent.";
inline constexpr char kCannotClaimAllURLsInExtent[] =
    "Cannot claim all URLs in an extent.";
inline constexpr char kCannotScriptGallery[] =
    "The extensions gallery cannot be scripted.";
inline constexpr char kCannotScriptNtp[] =
    "The New Tab Page cannot be scripted.";
inline constexpr char kCannotScriptSigninPage[] =
    "The sign-in page cannot be scripted.";
inline constexpr char16_t kChromeStyleInvalidForManifestV3[] =
    u"The chrome_style option cannot be used with manifest version 3.";
inline constexpr char kChromeVersionTooLow[] =
    "This extension requires * version * or greater.";
inline constexpr char kCommandActionIncorrectForManifestActionType[] =
    "The action commands in the manifest do not match the manifest's action "
    "type and were ignored.";
inline constexpr char kDeclarativeNetRequestPathDuplicates[] =
    "The same ruleset file appears multiple times with different IDs in the "
    "manifest 'declarative_net_request.rule_resources' key.";
inline constexpr char kDeclarativeNetRequestPermissionNeeded[] =
    "The extension requires the 'declarativeNetRequest' or the "
    "'declarativeNetRequestWithHostAccess' permission for the '*' manifest "
    "key.";
inline constexpr char16_t kDefaultStateShouldNotBeSet[] =
    u"The default_state key cannot be set for browser_action or page_action "
    "keys.";
inline constexpr char16_t kEmptyOmniboxKeyword[] =
    u"Invalid value for 'omnibox.keyword'. It must be non-empty.";
inline constexpr char kEnabledRulesetCountExceeded[] =
    "Invalid value for key '*.*': The number of enabled rulesets must be less "
    "than or equal to *.";
inline constexpr char kExecutionWorldRestrictedToMV3[] =
    "The 'world' property is restricted to extensions with 'manifest_version' "
    "set to 3 or higher.";
inline constexpr char kExpectString[] = "Expect string value.";
inline constexpr char kFileNotFound[] = "File not found: *.";
inline constexpr char kHasDifferentialFingerprint[] =
    "Manifest contains a differential_fingerprint key that will be overridden "
    "on extension update.";
inline constexpr char16_t kInvalidAboutPage[] =
    u"Invalid value for 'about_page'.";
inline constexpr char16_t kInvalidAboutPageExpectRelativePath[] =
    u"Invalid value for 'about_page'. Value must be a relative path.";
inline constexpr char kInvalidAction[] = "Invalid value for 'action'.";
inline constexpr char16_t kInvalidActionDefaultIcon[] =
    u"Invalid value for 'default_icon'.";
inline constexpr char16_t kInvalidActionDefaultPopup[] =
    u"Invalid type for 'default_popup'.";
inline constexpr char16_t kInvalidActionDefaultState[] =
    u"Invalid value for 'default_state'.";
inline constexpr char16_t kInvalidActionDefaultTitle[] =
    u"Invalid value for 'default_title'.";
inline constexpr char16_t kInvalidBackground[] =
    u"Invalid value for 'background_page'.";
inline constexpr char16_t kInvalidBackgroundAllowJsAccess[] =
    u"Invalid value for 'background.allow_js_access'.";
inline constexpr char16_t kInvalidBackgroundCombination[] =
    u"Only one of 'background.page', 'background.scripts', and "
    "'background.service_worker' can be specified.";
inline constexpr char kInvalidBackgroundScript[] =
    "Invalid value for 'background.scripts[*]'.";
inline constexpr char16_t kInvalidBackgroundScripts[] =
    u"Invalid value for 'background.scripts'.";
inline constexpr char16_t kInvalidBackgroundServiceWorkerScript[] =
    u"Invalid value for 'background.service_worker'.";
inline constexpr char16_t kInvalidBackgroundServiceWorkerType[] =
    u"Invalid value for 'background.type'.";
inline constexpr char16_t kInvalidBackgroundInHostedApp[] =
    u"Invalid value for 'background_page'. Hosted apps must specify an "
    "absolute HTTPS URL for the background page.";
inline constexpr char16_t kInvalidBackgroundPersistent[] =
    u"Invalid value for 'background.persistent'.";
inline constexpr char kInvalidBackgroundPersistentInPlatformApp[] =
    "Invalid value for 'app.background.persistent'. Packaged apps do not "
    "support persistent background pages and must use event pages.";
inline constexpr char16_t kInvalidBackgroundPersistentNoPage[] =
    u"Must specify one of background.page or background.scripts to use"
    " background.persistent.";
inline constexpr char kInvalidBrowserAction[] =
    "Invalid value for 'browser_action'.";
inline constexpr char kInvalidChromeURLOverrides[] =
    "Invalid value for 'chrome_url_overrides'.";
inline constexpr char16_t kInvalidCommandsKey[] =
    u"Invalid value for 'commands'.";
inline constexpr char16_t kInvalidContentCapabilities[] =
    u"Invalid value for 'content_capabilities'.";
inline constexpr char kInvalidContentCapabilitiesMatch[] =
    "Invalid content_capabilities URL pattern: *";
inline constexpr char kInvalidContentCapabilitiesMatchOrigin[] =
    "Domain wildcards are not allowed for content_capabilities URL patterns.";
inline constexpr char16_t kInvalidContentCapabilitiesParsedValue[] =
    u"Invalid content_capabilities parsing value. ";
inline constexpr char kInvalidContentCapabilitiesPermission[] =
    "Invalid content_capabilities permission: *.";
inline constexpr char kInvalidCSPInsecureValueIgnored[] =
    "'*': Ignored insecure CSP value \"*\" in directive '*'.";
inline constexpr char kInvalidCSPInsecureValueError[] =
    "'*': Insecure CSP value \"*\" in directive '*'.";
inline constexpr char kInvalidCSPMissingSecureSrc[] =
    "'*': CSP directive '*' must be specified (either explicitly, or "
    "implicitly via 'default-src') and must allowlist only secure resources.";
inline constexpr char kInvalidDefaultLocale[] =
    "Invalid value for default locale - locale name must be a string.";
inline constexpr char16_t kInvalidDefaultLocale16[] =
    u"Invalid value for default locale - locale name must be a string.";
inline constexpr char16_t kInvalidDescription[] =
    u"Invalid value for 'description'.";
inline constexpr char16_t kInvalidDevToolsPage[] =
    u"Invalid value for 'devtools_page'.";
inline constexpr char16_t kInvalidDisplayInLauncher[] =
    u"Invalid value for 'display_in_launcher'.";
inline constexpr char16_t kInvalidDisplayInNewTabPage[] =
    u"Invalid value for 'display_in_new_tab_page'.";
inline constexpr char kInvalidEmptyDictionary[] = "Empty dictionary for '*'.";
inline constexpr char kInvalidExcludeMatch[] =
    "Invalid value for 'content_scripts[*].exclude_matches[*]': *";
inline constexpr char kInvalidExcludeMatches[] =
    "Invalid value for 'content_scripts[*].exclude_matches'.";
inline constexpr char kInvalidExportPermissions[] =
    "Permissions are not allowed for extensions that export resources.";
inline constexpr char kInvalidExportAllowlistEmpty[] =
    "Empty 'export.allowlist' implies any extension can import this module.";
inline constexpr char kInvalidExportAllowlistString[] =
    "Invalid value for 'export.allowlist[*]'.";
inline constexpr char kInvalidExtensionOriginPopup[] =
    "The default_popup path specified in the manifest is invalid. Ensure it is "
    "a path to a file in this extension.";
inline constexpr char16_t kInvalidFileAccessList[] =
    u"Invalid value for 'file_access'.";
inline constexpr char kInvalidFileAccessValue[] =
    "Invalid value for 'file_access[*]'.";
inline constexpr char kInvalidFileBrowserHandler[] =
    "Invalid value for 'file_browser_handlers'.";
inline constexpr char16_t kInvalidFileBrowserHandler16[] =
    u"Invalid value for 'file_browser_handlers'.";
inline constexpr char16_t kInvalidFileBrowserHandlerId[] =
    u"Required value 'id' is missing or invalid.";
inline constexpr char kInvalidFileBrowserHandlerMissingPermission[] =
    "Declaring file browser handlers requires the fileBrowserHandler manifest "
    "permission.";
inline constexpr char16_t kInvalidFileFiltersList[] =
    u"Invalid value for 'file_filters'.";
inline constexpr char kInvalidFileFilterValue[] =
    "Invalid value for 'file_filters[*]'.";
inline constexpr char16_t kInvalidFileHandlers[] =
    u"Invalid value for 'file_handlers'.";
inline constexpr char kInvalidWebFileHandlers[] =
    "Invalid value for 'file_handlers[*]'. *";
inline constexpr char16_t kInvalidFileHandlersTooManyTypesAndExtensions[] =
    u"Too many MIME and extension file_handlers have been declared.";
inline constexpr char kInvalidFileHandlerExtension[] =
    "Invalid value for 'file_handlers[*].extensions'.";
inline constexpr char kInvalidFileHandlerExtensionElement[] =
    "Invalid value for 'file_handlers[*].extensions[*]'.";
inline constexpr char16_t kInvalidFileHandlerIncludeDirectories[] =
    u"Invalid value for 'include_directories'.";
inline constexpr char kInvalidFileHandlerNoTypeOrExtension[] =
    "'file_handlers[*]' must contain a non-empty 'types', 'extensions' "
    "or 'include_directories'.";
inline constexpr char kInvalidFileHandlerType[] =
    "Invalid value for 'file_handlers[*].types'.";
inline constexpr char kInvalidFileHandlerTypeElement[] =
    "Invalid value for 'file_handlers[*].types[*]'.";
inline constexpr char kInvalidFileHandlerVerb[] =
    "Invalid value for 'file_handlers[*].verb'.";
inline constexpr char kInvalidHomepageOverrideURL[] =
    "Invalid value for overriding homepage url: '[*]'.";
inline constexpr char kInvalidHomepageURL[] =
    "Invalid value for homepage url: '[*]'.";
inline constexpr char kInvalidHostPermission[] = "Invalid value for '*[*]'.";
inline constexpr char kInvalidHostPermissions[] = "Invalid value for '*'.";
inline constexpr char kInvalidIconKey[] = "Invalid key in icons: \"*\".";
inline constexpr char kInvalidIconPath[] = "Invalid value for 'icons[\"*\"]'.";
inline constexpr char16_t kInvalidIcons[] = u"Invalid value for 'icons'.";
inline constexpr char16_t kInvalidImportAndExport[] =
    u"Simultaneous 'import' and 'export' are not allowed.";
inline constexpr char kInvalidImportId[] = "Invalid value for 'import[*].id'.";
inline constexpr char kInvalidImportVersion[] =
    "Invalid value for 'import[*].minimum_version'.";
inline constexpr char kInvalidImportRepeatedImport[] =
    "There are multiple occurences of the same extension ID in 'import'. Only "
    "one version will be installed.";
inline constexpr char kInvalidInputComponents[] =
    "Invalid value for 'input_components'";
inline constexpr char16_t kInvalidInputComponents16[] =
    u"Invalid value for 'input_components'";
inline constexpr char kInvalidInputComponentLayoutName[] =
    "Invalid value for 'input_components[*].layouts[*]";
inline constexpr char kInvalidInputComponentName[] =
    "Invalid value for 'input_components[*].name";
inline constexpr char kInvalidInputView[] = "Invalid value for 'input_view'.";
inline constexpr char16_t kInvalidIsolation[] =
    u"Invalid value for 'app.isolation'.";
inline constexpr char kInvalidIsolationValue[] =
    "Invalid value for 'app.isolation[*]'.";
inline constexpr char16_t kInvalidKey[] = u"Value 'key' is missing or invalid.";
inline constexpr char kInvalidKeyBinding[] =
    "Invalid value for 'commands[*].*': *.";
inline constexpr char kInvalidKeyBindingDescription[] =
    "Invalid value for 'commands[*].description'.";
inline constexpr char kInvalidKeyBindingDictionary[] =
    "Contents of 'commands[*]' invalid.";
inline constexpr char kInvalidKeyBindingMediaKeyWithModifier[] =
    "Media key cannot have any modifier for 'commands[*].*': *.";
inline constexpr char kInvalidKeyBindingMissingPlatform[] =
    "Could not find key specification for 'command[*].*': Either specify a key "
    "for '*', or specify a default key.";
inline constexpr char kInvalidKeyBindingTooMany[] =
    "Too many shortcuts specified for 'commands': The maximum is *.";
inline constexpr char kInvalidKeyBindingUnknownPlatform[] =
    "Unknown platform for 'command[*]': *. Valid values are: 'windows', 'mac'"
    " 'chromeos', 'linux' and 'default'.";
inline constexpr char16_t kInvalidKioskAlwaysUpdate[] =
    u"Invalid value for 'kiosk.always_update'.";
inline constexpr char16_t kInvalidKioskEnabled[] =
    u"Invalid value for 'kiosk_enabled'.";
inline constexpr char16_t kInvalidKioskOnly[] =
    u"Invalid value for 'kiosk_only'.";
inline constexpr char16_t kInvalidKioskOnlyButNotEnabled[] =
    u"The 'kiosk_only' key is set, but 'kiosk_enabled' is not set.";
inline constexpr char16_t kInvalidKioskRequiredPlatformVersion[] =
    u"Invalid value for 'kiosk.required_platform_version'";
inline constexpr char16_t kInvalidKioskSecondaryApps[] =
    u"Invalid value for 'kiosk_secondary_apps'";
inline constexpr char16_t kInvalidKioskSecondaryAppsBadAppEntry[] =
    u"Invalid app item for 'kiosk_secondary_apps'";
inline constexpr char kInvalidKioskSecondaryAppsDuplicateApp[] =
    "Duplicate app id in 'kiosk_secondary_apps': '*'.";
inline constexpr char kInvalidKioskSecondaryAppsPropertyUnavailable[] =
    "Property '*' not allowed for 'kiosk_secondary_apps' item '*'.";
inline constexpr char16_t kInvalidLaunchContainer[] =
    u"Invalid value for 'app.launch.container'.";
inline constexpr char kInvalidLaunchValue[] = "Invalid value for '*'.";
inline constexpr char kInvalidLaunchValueContainer[] =
    "Invalid container type for '*'.";
inline constexpr char kInvalidLinkedAppIcon[] =
    "Invalid linked app icon. Must be a dictionary";
inline constexpr char kInvalidLinkedAppIconSize[] =
    "Invalid 'size' for linked app icon. Must be an integer";
inline constexpr char kInvalidLinkedAppIconURL[] =
    "Invalid 'url' for linked app icon. Must be a string that is a valid URL";
inline constexpr char kInvalidLinkedAppIcons[] =
    "Invalid 'app.linked_icons'. Must be an array";
inline constexpr char kInvalidManifest[] = "Manifest file is invalid";
inline constexpr char kInvalidManifestKey[] = "Invalid value for '*'.";
inline constexpr char kInvalidManifestVersionMissingKey[] =
    "Missing 'manifest_version' key. Its value must be an integer *. "
    "See developer.chrome.com/*/manifestVersion for details.";
inline constexpr char kInvalidManifestVersionUnsupported[] =
    "Invalid value for 'manifest_version'. Must be an integer *. "
    "See developer.chrome.com/*/manifestVersion for details.";
inline constexpr char kInvalidMatch[] =
    "Invalid value for 'content_scripts[*].matches[*]': *";
inline constexpr char kInvalidMatchCount[] =
    "Invalid value for 'content_scripts[*].matches'. There must be at least "
    "one match specified.";
inline constexpr char kInvalidMatches[] =
    "Required value 'content_scripts[*].matches' is missing or invalid.";
inline constexpr char16_t kInvalidMIMETypes[] =
    u"Invalid value for 'mime_types'";
inline constexpr char16_t kInvalidMimeTypesHandler[] =
    u"Invalid value for 'mime_types'.";
inline constexpr char16_t kInvalidMinimumChromeVersion[] =
    u"Invalid value for 'minimum_chrome_version'.";
inline constexpr char16_t kInvalidNaClModules[] =
    u"Invalid value for 'nacl_modules'.";
inline constexpr char kInvalidNaClModulesMIMEType[] =
    "Invalid value for 'nacl_modules[*].mime_type'.";
inline constexpr char kInvalidNaClModulesPath[] =
    "Invalid value for 'nacl_modules[*].path'.";
inline constexpr char kInvalidName[] =
    "Required value 'name' is missing or invalid.";
inline constexpr char16_t kInvalidName16[] =
    u"Required value 'name' is missing or invalid.";
inline constexpr char16_t kInvalidNativelyConnectable[] =
    u"Invalid natively_connectable. Must be a list.";
inline constexpr char kInvalidNativelyConnectableValue[] =
    "Invalid natively_connectable value. Must be a non-empty string.";
inline constexpr char16_t kInvalidNativelyConnectableValue16[] =
    u"Invalid natively_connectable value. Must be a non-empty string.";
inline constexpr char16_t kInvalidOAuth2ClientId[] =
    u"Invalid value for 'oauth2.client_id'.";
inline constexpr char16_t kInvalidOfflineEnabled[] =
    u"Invalid value for 'offline_enabled'.";
inline constexpr char kInvalidOptionsPage[] = "Invalid value for '*'.";
inline constexpr char16_t kInvalidOptionsPageExpectUrlInPackage[] =
    u"Invalid value for 'options_page'.  Value must be a relative path.";
inline constexpr char16_t kInvalidOptionsPageInHostedApp[] =
    u"Invalid value for 'options_page'. Hosted apps must specify an "
    "absolute URL.";
inline constexpr char kInvalidOptionsUIChromeStyle[] =
    "Invalid value for 'options_ui.chrome_style'.";
inline constexpr char kInvalidOptionsUIOpenInTab[] =
    "Invalid value for 'options_ui.open_in_tab'.";
inline constexpr char kInvalidPageAction[] = "Invalid value for 'page_action'.";
inline constexpr char kInvalidPermission[] =
    "Invalid value for 'permissions[*]'.";
inline constexpr char kInvalidPermissionScheme[] = "Invalid scheme for '*[*]'.";
inline constexpr char kInvalidPermissionWithDetail[] =
    "Invalid value for 'permissions[*]': *.";
inline constexpr char16_t kInvalidPermissions[] =
    u"Invalid value for 'permissions'.";
inline constexpr char16_t kInvalidReplacementWebApp[] =
    u"Invalid value for 'replacement_web_app'.";
inline constexpr char kInvalidRulesetID[] =
    "'*.*': Invalid 'id' specified for Ruleset at index *. The ID must be "
    "non-empty, unique and must not start with '_'.";
inline constexpr char16_t kInvalidSandboxedPagesList[] =
    u"Invalid value for 'sandbox.pages'.";
inline constexpr char kInvalidSandboxedPage[] =
    "Invalid value for 'sandbox.pages[*]'.";
inline constexpr char kInvalidSearchEngineMissingKeys[] =
    "Missing or invalid value for "
    "'chrome_settings_overrides.search_provider.*.";
inline constexpr char kInvalidSearchEngineURL[] =
    "Invalid URL [*] for 'chrome_settings_overrides.search_provider'.";
inline constexpr char16_t kInvalidShortName[] =
    u"Invalid value for 'short_name'.";
inline constexpr char kInvalidStartupOverrideURL[] =
    "Invalid value for overriding startup URL: '[*]'.";
inline constexpr char16_t kInvalidSystemIndicator[] =
    u"Invalid value for 'system_indicator'.";
inline constexpr char16_t kInvalidTheme[] = u"Invalid value for 'theme'.";
inline constexpr char16_t kInvalidThemeColors[] =
    u"Invalid value for theme colors - colors must be integers";
inline constexpr char16_t kInvalidThemeImages[] =
    u"Invalid value for theme images - images must be strings.";
inline constexpr char kInvalidThemeImagesMissing[] =
    "An image specified in the theme is missing.";
inline constexpr char16_t kInvalidThemeTints[] =
    u"Invalid value for theme images - tints must be decimal numbers.";
inline constexpr char kInvalidTrialTokensNonEmptyList[] =
    "Invalid value for 'trial_tokens'. Must be a non-empty list.";
inline constexpr char kInvalidTrialTokensValue[] =
    "Invalid element in 'trial_tokens'. Must be a non-empty string.";
inline constexpr char kInvalidTrialTokensValueDuplicate[] =
    "Duplicate element in 'trial_tokens': '%s'.";
inline constexpr char kInvalidTrialTokensValueTooLong[] =
    "Invalid element in 'trial_tokens'. Token must not be longer than %zu.";
inline constexpr char kInvalidTrialTokensTooManyTokens[] =
    "Invalid value for 'trial_tokens': too many tokens. Only the first %zu "
    "will be processed.";
inline constexpr char16_t kInvalidTts[] = u"Invalid value for 'tts_engine'.";
inline constexpr char16_t kInvalidTtsSampleRateFormat[] =
    u"Invalid format for tts_engine.sample_rate: expected integer.";
inline constexpr char kInvalidTtsSampleRateRange[] =
    "Invalid tts_engine.sample_rate: out of range. Expected sample_rate >= %d "
    "and sample_rate <= %d.";
inline constexpr char16_t kInvalidTtsBufferSizeFormat[] =
    u"Invalid format for tts_engine.buffer_size: expected integer.";
inline constexpr char kInvalidTtsBufferSizeRange[] =
    "Invalid tts_engine.buffer_size: out of range. Expected buffer_size >= %d "
    "and buffer_size <= %d.";
inline constexpr char16_t kInvalidTtsRequiresSampleRateAndBufferSize[] =
    u"Invalid tts_engine: requires both sample_rate and buffer_size if either "
    "is specified.";
inline constexpr char16_t kInvalidTtsVoices[] =
    u"Invalid value for 'tts_engine.voices'.";
inline constexpr char16_t kInvalidTtsVoicesEventTypes[] =
    u"Invalid value for 'tts_engine.voices[*].event_types'.";
inline constexpr char16_t kInvalidTtsVoicesLang[] =
    u"Invalid value for 'tts_engine.voices[*].lang'.";
inline constexpr char16_t kInvalidTtsVoicesRemote[] =
    u"Invalid value for 'tts_engine.voices[*].remote'.";
inline constexpr char16_t kInvalidTtsVoicesVoiceName[] =
    u"Invalid value for 'tts_engine.voices[*].voice_name'.";
inline constexpr char kInvalidURLHandlerPattern[] =
    "Invalid value for 'url_handlers[*].matches'.";
inline constexpr char kInvalidURLHandlerPatternElement[] =
    "Invalid value for 'url_handlers[*]'.";
inline constexpr char16_t kInvalidURLHandlerPatternElement16[] =
    u"Invalid value for 'url_handlers[*]'.";
inline constexpr char16_t kInvalidURLHandlerTitle[] =
    u"Invalid value for 'url_handlers[*].title'.";
inline constexpr char16_t kInvalidURLHandlers[] =
    u"Invalid value for 'url_handlers'.";
inline constexpr char kInvalidURLPatternError[] = "Invalid url pattern '*'";
inline constexpr char kInvalidUpdateURL[] =
    "Invalid value for update url: '[*]'.";
inline constexpr char16_t kInvalidVersion[] =
    u"Required value 'version' is missing or invalid. It must be between 1-4 "
    "dot-separated integers each between 0 and 65536.";
inline constexpr char16_t kInvalidVersionName[] =
    u"Invalid value for 'version_name'.";
inline constexpr char kInvalidWebAccessibleResourcesList[] =
    "Invalid value for 'web_accessible_resources'.";
inline constexpr char kInvalidWebAccessibleResource[] =
    "Invalid value for 'web_accessible_resources[*]'. *";
inline constexpr char kInvalidSidePanel[] = "Invalid value for 'side_panel'. *";
inline constexpr char16_t kInvalidWebview[] = u"Invalid value for 'webview'.";
inline constexpr char16_t kInvalidWebviewAccessibleResourcesList[] =
    u"Invalid value for'webview.accessible_resources'.";
inline constexpr char kInvalidWebviewAccessibleResource[] =
    "Invalid value for 'webview.accessible_resources[*]'.";
inline constexpr char kInvalidWebviewPartition[] =
    "Invalid value for 'webview.partitions[*]'.";
inline constexpr char kInvalidWebviewPartitionName[] =
    "Invalid value for 'webview.partitions[*].name'.";
inline constexpr char16_t kInvalidWebviewPartitionsList[] =
    u"Invalid value for 'webview.partitions'.";
inline constexpr char kInvalidWebURL[] = "Invalid value for 'app.urls[*]': *";
inline constexpr char kInvalidWebURLs[] = "Invalid value for 'app.urls'.";
inline constexpr char kInvalidZipHash[] =
    "Required key 'zip_hash' is missing or invalid.";
inline constexpr char kKeyIsDeprecatedWithReplacement[] =
    "Key \"*\" is deprecated.  Key \"*\" should be used instead.";
inline constexpr char16_t kLaunchPathAndExtentAreExclusive[] =
    u"The 'app.launch.local_path' and 'app.urls' keys cannot both be set.";
inline constexpr char16_t kLaunchPathAndURLAreExclusive[] =
    u"The 'app.launch.local_path' and 'app.launch.web_url' keys cannot "
    "both be set.";
inline constexpr char16_t kLaunchURLRequired[] =
    u"Either 'app.launch.local_path' or 'app.launch.web_url' is required.";
inline constexpr char kLocalesInvalidLocale[] = "Invalid locale file '*': *";
inline constexpr char16_t kLocalesMessagesFileMissing[] =
    u"Messages file is missing for locale.";
inline constexpr char kLocalesNoDefaultMessages[] =
    "Default locale is defined but default data couldn't be loaded.";
inline constexpr char kLocalesNoValidLocaleNamesListed[] =
    "No valid locale name could be found in _locales directory.";
inline constexpr char kLocalesTreeMissing[] =
    "Default locale was specified, but _locales subtree is missing.";
inline constexpr char kManifestParseError[] = "Manifest is not valid JSON.";
inline constexpr char kManifestUnreadable[] =
    "Manifest file is missing or unreadable";
inline constexpr char kManifestV2IsDeprecatedWarning[] =
    "Manifest version 2 is deprecated, and support will be removed in 2024. "
    "See https://developer.chrome.com/docs/extensions/develop/migrate/mv2-deprecation-timeline"
    " for details.";
inline constexpr char kManifestVersionTooHighWarning[] =
    "The maximum currently-supported manifest version is *, but this is *.  "
    "Certain features may not work as expected.";
inline constexpr char16_t kMatchOriginAsFallbackCantHavePaths[] =
    u"The path component for scripts with 'match_origin_as_fallback' must be "
    "'*'.";
inline constexpr char kMissingFile[] =
    "At least one js or css file is required for 'content_scripts[*]'.";
inline constexpr char16_t kMultipleOverrides[] =
    u"An extension cannot override more than one page.";
inline constexpr char16_t kNPAPIPluginsNotSupported[] =
    u"NPAPI plugins are not supported.";
inline constexpr char kNoWildCardsInPaths[] =
    "Wildcards are not allowed in extent URL pattern paths.";
inline constexpr char kNonexistentDefaultPopup[] =
    "The default_popup file in the manifest doesn't exist. Confirm it exists "
    "and then reload the extension.";
inline constexpr char16_t kOneUISurfaceOnly[] =
    u"Only one of 'browser_action', 'page_action', and 'app' can be specified.";
inline constexpr char kPageCaptureNeeded[] =
    "'pageCapture' permission is required.";
inline constexpr char kPermissionCannotBeOptional[] =
    "Permission '*' cannot be listed as optional. This permission will be "
    "omitted.";
inline constexpr char kPermissionMarkedOptionalAndRequired[] =
    "Optional permission '*' is redundant with the required permissions;"
    "this permission will be omitted.";
inline constexpr char kPermissionNotAllowed[] =
    "Access to permission '*' denied.";
inline constexpr char kPermissionUnknownOrMalformed[] =
    "Permission '*' is unknown or URL pattern is malformed.";
inline constexpr char kPluginsRequirementDeprecated[] =
    "The \"plugins\" requirement is deprecated.";
inline constexpr char kReservedMessageFound[] =
    "Reserved key * found in message catalog.";
inline constexpr char kRulesFileIsInvalid[] =
    "Invalid value for key '*.*': The provided path '*' is invalid.";
inline constexpr char kRulesetCountExceeded[] =
    "Invalid value for key '*.*': The number of rulesets must be less than or "
    "equal to *.";
inline constexpr char16_t kSandboxPagesCSPKeyNotAllowed[] =
    u"The Content Security Policy for sandboxed pages should be specified in "
    "'content_security_policy.sandbox'.";
inline constexpr char kSidePanelManifestDefaultPathError[] =
    "Side panel file path must exist.";
inline constexpr char16_t
    kTransientBackgroundConflictsWithPersistentBackground[] =
        u"The 'transientBackground' permission cannot be used with a "
        "persistent background page.";
inline constexpr char kUnrecognizedManifestKey[] =
    "Unrecognized manifest key '*'.";
inline constexpr char kUnrecognizedManifestProperty[] =
    "Unrecognized property '*' of manifest key '*'.";
inline constexpr char16_t kWebRequestConflictsWithLazyBackground[] =
    u"The 'webRequest' API cannot be used with event pages.";
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr char kDuplicateActionHandlerFound[] =
    "'action_handlers' list contains duplicate entries for the action: \"*\".";
inline constexpr char kIllegalPlugins[] =
    "Extensions cannot install plugins on Chrome OS.";
inline constexpr char16_t kInvalidActionHandlerDictionary[] =
    u"Invalid action handler dictionary in 'action_handlers': 'action' key "
    "missing.";
inline constexpr char kInvalidActionHandlersActionType[] =
    "Invalid entry in 'action_handlers': \"*\".";
inline constexpr char16_t kInvalidActionHandlersType[] =
    u"Invalid value for 'action_handlers'. Value must be a list of strings or "
    "a dictionary with 'action' key.";
inline constexpr char16_t kInvalidFileSystemProviderMissingCapabilities[] =
    u"The 'fileSystemProvider' permission requires the "
    "'file_system_provider_capabilities' section to be specified in the "
    "manifest.";
inline constexpr char kInvalidFileSystemProviderMissingPermission[] =
    "The 'file_system_provider_capabilities' section requires the "
    "'fileSystemProvider' permission to be specified in the manifest.";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace manifest_errors

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_CONSTANTS_H_
