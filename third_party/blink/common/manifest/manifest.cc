// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/manifest/manifest_invariants.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/third_party/mozilla/url_parse.h"

namespace blink {

Manifest::ImageResource::ImageResource() = default;

Manifest::ImageResource::ImageResource(const ImageResource& other) = default;

Manifest::ImageResource::~ImageResource() = default;

bool Manifest::ImageResource::operator==(
    const Manifest::ImageResource& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.src, item.type, item.sizes);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::ShortcutItem::ShortcutItem() = default;

Manifest::ShortcutItem::~ShortcutItem() = default;

bool Manifest::ShortcutItem::operator==(const ShortcutItem& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.name, item.short_name, item.description, item.url,
                    item.icons, item.icons_localized, item.name_localized,
                    item.short_name_localized, item.description_localized);
  };
  return AsTuple(*this) == AsTuple(other);
}

bool Manifest::FileFilter::operator==(const FileFilter& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.name, item.accept);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::ShareTargetParams::ShareTargetParams() = default;

Manifest::ShareTargetParams::~ShareTargetParams() = default;

bool Manifest::ShareTargetParams::operator==(
    const ShareTargetParams& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.title, item.text, item.url, item.files);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::ShareTarget::ShareTarget() = default;

Manifest::ShareTarget::~ShareTarget() = default;

bool Manifest::ShareTarget::operator==(const ShareTarget& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.action, item.method, item.enctype, item.params);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::RelatedApplication::RelatedApplication() = default;

Manifest::RelatedApplication::~RelatedApplication() = default;

bool Manifest::RelatedApplication::operator==(
    const RelatedApplication& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.platform, item.url, item.id);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::LaunchHandler::LaunchHandler() = default;

Manifest::LaunchHandler::LaunchHandler(std::optional<ClientMode> client_mode)
    : client_mode_(client_mode) {}

// See https://wicg.github.io/web-app-launch/#dfn-process-the-client_mode-member
// for more details.
Manifest::LaunchHandler::ClientMode
Manifest::LaunchHandler::parsed_client_mode() const {
  return client_mode_.value_or(Manifest::LaunchHandler::ClientMode::kAuto);
}

bool Manifest::LaunchHandler::client_mode_valid_and_specified() const {
  return client_mode_.has_value();
}

bool Manifest::LaunchHandler::operator==(const LaunchHandler& other) const {
  return parsed_client_mode() == other.parsed_client_mode();
}

bool Manifest::LaunchHandler::TargetsExistingClients() const {
  switch (parsed_client_mode()) {
    case ClientMode::kAuto:
    case ClientMode::kNavigateNew:
      return false;
    case ClientMode::kNavigateExisting:
    case ClientMode::kFocusExisting:
      return true;
  }
}

bool Manifest::LaunchHandler::NeverNavigateExistingClients() const {
  switch (parsed_client_mode()) {
    case ClientMode::kAuto:
    case ClientMode::kNavigateNew:
    case ClientMode::kNavigateExisting:
      return false;
    case ClientMode::kFocusExisting:
      return true;
  }
}

Manifest::TranslationItem::TranslationItem() = default;

Manifest::TranslationItem::~TranslationItem() = default;

bool Manifest::TranslationItem::operator==(const TranslationItem& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.name, item.short_name, item.description);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::HomeTabParams::HomeTabParams() = default;

Manifest::HomeTabParams::~HomeTabParams() = default;

bool Manifest::HomeTabParams::operator==(const HomeTabParams& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.icons, item.scope_patterns);
  };
  return AsTuple(*this) == AsTuple(other);
}

Manifest::NewTabButtonParams::NewTabButtonParams() = default;

Manifest::NewTabButtonParams::~NewTabButtonParams() = default;

bool Manifest::NewTabButtonParams::operator==(
    const NewTabButtonParams& other) const {
  return url == other.url;
}

Manifest::TabStrip::TabStrip() = default;

Manifest::TabStrip::~TabStrip() = default;

bool Manifest::TabStrip::operator==(const TabStrip& other) const {
  auto AsTuple = [](const auto& item) {
    return std::tie(item.home_tab, item.new_tab_button);
  };
  return AsTuple(*this) == AsTuple(other);
}

// static
Manifest::DisplayOverride Manifest::DisplayOverride::Create(
    blink::mojom::DisplayMode display_mode) {
  return DisplayOverride(display_mode, {});
}

// static
Manifest::DisplayOverride Manifest::DisplayOverride::CreateUnframed(
    std::vector<SafeUrlPattern> url_patterns) {
  return DisplayOverride(blink::mojom::DisplayMode::kUnframed,
                         std::move(url_patterns));
}

Manifest::DisplayOverride::DisplayOverride(
    blink::mojom::DisplayMode display_mode,
    std::vector<SafeUrlPattern> patterns)
    : display_(display_mode), url_patterns_(std::move(patterns)) {
  CHECK(url_patterns_.empty() ||
        display_ == blink::mojom::DisplayMode::kUnframed)
      << "url_patterns is not allowed in display modes other than 'unframed'";
}

Manifest::DisplayOverride::DisplayOverride() = default;
Manifest::DisplayOverride::DisplayOverride(const DisplayOverride& other) =
    default;
Manifest::DisplayOverride::DisplayOverride(DisplayOverride&& other) = default;
Manifest::DisplayOverride& Manifest::DisplayOverride::operator=(
    const DisplayOverride& other) = default;
Manifest::DisplayOverride& Manifest::DisplayOverride::operator=(
    DisplayOverride&& other) = default;
Manifest::DisplayOverride::~DisplayOverride() = default;

bool Manifest::DisplayOverride::operator==(const DisplayOverride& other) const =
    default;

// Manifest implementation

Manifest::Manifest() = default;
Manifest::Manifest(mojo::DefaultConstruct::Tag) {}

Manifest::Manifest(const Manifest&) = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest::Manifest(Manifest&&) noexcept = default;
Manifest& Manifest::operator=(Manifest&&) noexcept = default;
Manifest::~Manifest() = default;

bool Manifest::operator==(const Manifest& other) const {
  // LINT.IfChange(ManifestEquality)
  return manifest_url_ == other.manifest_url_ && dir_ == other.dir_ &&
         name_ == other.name_ && short_name_ == other.short_name_ &&
         description_ == other.description_ && id_ == other.id_ &&
         has_custom_id_ == other.has_custom_id_ &&
         start_url_ == other.start_url_ &&
         has_valid_specified_start_url_ ==
             other.has_valid_specified_start_url_ &&
         display_ == other.display_ &&
         display_override_ == other.display_override_ &&
         orientation_ == other.orientation_ && icons_ == other.icons_ &&
         screenshots_ == other.screenshots_ && shortcuts_ == other.shortcuts_ &&
         share_target_ == other.share_target_ &&
         file_handlers_ == other.file_handlers_ &&
         protocol_handlers_ == other.protocol_handlers_ &&
         scope_extensions_ == other.scope_extensions_ &&
         lock_screen_ == other.lock_screen_ &&
         note_taking_ == other.note_taking_ &&
         related_applications_ == other.related_applications_ &&
         prefer_related_applications_ == other.prefer_related_applications_ &&
         migrate_from_ == other.migrate_from_ &&
         migrate_to_ == other.migrate_to_ &&
         theme_color_ == other.theme_color_ &&
         background_color_ == other.background_color_ &&
         gcm_sender_id_ == other.gcm_sender_id_ && scope_ == other.scope_ &&
         update_manifest_url_ == other.update_manifest_url_ &&
         launch_handler_ == other.launch_handler_ &&
         translations_ == other.translations_ &&
         dark_theme_color_ == other.dark_theme_color_ &&
         dark_background_color_ == other.dark_background_color_ &&
         tab_strip_ == other.tab_strip_ && version_ == other.version_ &&
         name_localized_ == other.name_localized_ &&
         short_name_localized_ == other.short_name_localized_ &&
         description_localized_ == other.description_localized_ &&
         icons_localized_ == other.icons_localized_;
  // LINT.ThenChange(
  //   :ManifestToBuilder,
  //   :ManifestBuilderBuild,
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestFields,
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestBuilderFields,
  //   //third_party/blink/common/manifest/manifest_mojom_traits.cc:ManifestStructTraitsRead
  // )
}

// static
GURL Manifest::GetDefaultScope(const GURL& document_url) {
  if (!document_url.is_valid()) {
    return GURL();
  }
  const std::string& spec = document_url.possibly_invalid_spec();
  const url::Parsed& parsed = document_url.parsed_for_possibly_invalid_spec();
  int end_pos = 0;
  if (parsed.path.is_valid()) {
    url::Component filename;
    url::ExtractFileName(spec, parsed.path, &filename);
    end_pos = filename.begin;
  } else {
    end_pos = parsed.CountCharactersBefore(url::Parsed::PATH, false);
  }
  return GURL(spec.substr(0, end_pos));
}

// static
std::optional<Manifest> Manifest::CreateDefault(const GURL& document_url) {
  if (!document_url.is_valid() || document_url.SchemeIs("about")) {
    return std::nullopt;
  }
  url::Origin origin = url::Origin::Create(document_url);
  if (origin.opaque()) {
    return std::nullopt;
  }

  GURL start_url = document_url;
  GURL id = document_url.GetWithoutRef();
  GURL scope = GetDefaultScope(document_url);

  auto result =
      Builder(std::move(start_url), std::move(id), std::move(scope)).Build();
  if (!result.has_value()) {
    return std::nullopt;
  }
  return std::move(*result);
}

Manifest::Builder Manifest::ToBuilder() const {
  // LINT.IfChange(ManifestToBuilder)
  Builder builder(start_url_, id_, scope_);
  builder.SetManifestUrl(manifest_url_);
  builder.SetDir(dir_);
  builder.SetName(name_);
  builder.SetShortName(short_name_);
  builder.SetDescription(description_);
  builder.SetHasCustomId(has_custom_id_);
  builder.SetHasValidSpecifiedStartUrl(has_valid_specified_start_url_);
  builder.SetDisplay(display_);
  builder.SetDisplayOverride(display_override_);
  builder.SetOrientation(orientation_);
  builder.SetIcons(icons_);
  builder.SetScreenshots(screenshots_);
  builder.SetShortcuts(shortcuts_);
  builder.SetShareTarget(share_target_);
  builder.SetFileHandlers(file_handlers_);
  builder.SetProtocolHandlers(protocol_handlers_);
  builder.SetScopeExtensions(scope_extensions_);
  builder.SetLockScreen(lock_screen_);
  builder.SetNoteTaking(note_taking_);
  builder.SetRelatedApplications(related_applications_);
  builder.SetPreferRelatedApplications(prefer_related_applications_);
  builder.SetMigrateFrom(migrate_from_);
  builder.SetMigrateTo(migrate_to_);
  builder.SetThemeColor(theme_color_);
  builder.SetBackgroundColor(background_color_);
  builder.SetGcmSenderId(gcm_sender_id_);
  builder.SetUpdateManifestUrl(update_manifest_url_);
  builder.SetLaunchHandler(launch_handler_);
  builder.SetTranslations(translations_);
  builder.SetDarkThemeColor(dark_theme_color_);
  builder.SetDarkBackgroundColor(dark_background_color_);
  builder.SetTabStrip(tab_strip_);
  builder.SetVersion(version_);
  builder.SetNameLocalized(name_localized_);
  builder.SetShortNameLocalized(short_name_localized_);
  builder.SetDescriptionLocalized(description_localized_);
  builder.SetIconsLocalized(icons_localized_);
  // LINT.ThenChange(
  //   :ManifestEquality,
  //   :ManifestBuilderBuild,
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestFields,
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestBuilderFields,
  //   //third_party/blink/common/manifest/manifest_mojom_traits.cc:ManifestStructTraitsRead
  // )
  return builder;
}

// Manifest::Builder implementation

Manifest::Builder::Builder(GURL start_url, GURL id, GURL scope)
    : id_(std::move(id)),
      start_url_(std::move(start_url)),
      scope_(std::move(scope)) {}

Manifest::Builder::Builder(GURL start_url)
    : id_(start_url.GetWithoutRef()),
      start_url_(std::move(start_url)),
      scope_(Manifest::GetDefaultScope(start_url_)) {}

Manifest::Builder::~Builder() = default;
Manifest::Builder::Builder(const Builder&) = default;
Manifest::Builder::Builder(Builder&&) noexcept = default;
Manifest::Builder& Manifest::Builder::operator=(const Builder&) = default;
Manifest::Builder& Manifest::Builder::operator=(Builder&&) noexcept = default;

Manifest::Builder& Manifest::Builder::SetManifestUrl(GURL manifest_url) {
  manifest_url_ = std::move(manifest_url);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDir(
    mojom::Manifest_TextDirection dir) {
  dir_ = dir;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetName(
    std::optional<std::u16string> name) {
  name_ = std::move(name);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShortName(
    std::optional<std::u16string> short_name) {
  short_name_ = std::move(short_name);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDescription(
    std::optional<std::u16string> description) {
  description_ = std::move(description);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetId(GURL id) {
  id_ = std::move(id);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetHasCustomId(bool has_custom_id) {
  has_custom_id_ = has_custom_id;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetStartUrl(GURL start_url) {
  start_url_ = std::move(start_url);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetHasValidSpecifiedStartUrl(
    bool has_valid_specified_start_url) {
  has_valid_specified_start_url_ = has_valid_specified_start_url;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDisplay(mojom::DisplayMode display) {
  display_ = display;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDisplayOverride(
    std::vector<DisplayOverride> display_override) {
  display_override_ = std::move(display_override);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetOrientation(
    device::mojom::ScreenOrientationLockType orientation) {
  orientation_ = orientation;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetIcons(
    std::vector<ImageResource> icons) {
  icons_ = std::move(icons);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetScreenshots(
    std::vector<Screenshot> screenshots) {
  screenshots_ = std::move(screenshots);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShortcuts(
    std::vector<ShortcutItem> shortcuts) {
  shortcuts_ = std::move(shortcuts);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShareTarget(
    std::optional<ShareTarget> share_target) {
  share_target_ = std::move(share_target);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetFileHandlers(
    std::vector<FileHandler> file_handlers) {
  file_handlers_ = std::move(file_handlers);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetProtocolHandlers(
    std::vector<ProtocolHandler> protocol_handlers) {
  protocol_handlers_ = std::move(protocol_handlers);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetScopeExtensions(
    std::vector<ScopeExtension> scope_extensions) {
  scope_extensions_ = std::move(scope_extensions);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetLockScreen(
    std::optional<LockScreen> lock_screen) {
  lock_screen_ = std::move(lock_screen);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetNoteTaking(
    std::optional<NoteTaking> note_taking) {
  note_taking_ = std::move(note_taking);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetRelatedApplications(
    std::vector<RelatedApplication> related_applications) {
  related_applications_ = std::move(related_applications);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetPreferRelatedApplications(
    bool prefer_related_applications) {
  prefer_related_applications_ = prefer_related_applications;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetMigrateFrom(
    std::vector<MigrateFrom> migrate_from) {
  migrate_from_ = std::move(migrate_from);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetMigrateTo(
    std::optional<MigrateTo> migrate_to) {
  migrate_to_ = std::move(migrate_to);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetThemeColor(
    std::optional<SkColor> theme_color) {
  theme_color_ = theme_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetBackgroundColor(
    std::optional<SkColor> background_color) {
  background_color_ = background_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetGcmSenderId(
    std::optional<std::u16string> gcm_sender_id) {
  gcm_sender_id_ = std::move(gcm_sender_id);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetScope(GURL scope) {
  scope_ = std::move(scope);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetUpdateManifestUrl(
    std::optional<GURL> update_manifest_url) {
  update_manifest_url_ = std::move(update_manifest_url);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetLaunchHandler(
    std::optional<LaunchHandler> launch_handler) {
  launch_handler_ = std::move(launch_handler);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetTranslations(
    base::flat_map<std::u16string, TranslationItem> translations) {
  translations_ = std::move(translations);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDarkThemeColor(
    std::optional<SkColor> dark_theme_color) {
  dark_theme_color_ = dark_theme_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDarkBackgroundColor(
    std::optional<SkColor> dark_background_color) {
  dark_background_color_ = dark_background_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetTabStrip(
    std::optional<TabStrip> tab_strip) {
  tab_strip_ = std::move(tab_strip);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetVersion(
    std::optional<std::u16string> version) {
  version_ = std::move(version);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetNameLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        name_localized) {
  name_localized_ = std::move(name_localized);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShortNameLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        short_name_localized) {
  short_name_localized_ = std::move(short_name_localized);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDescriptionLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        description_localized) {
  description_localized_ = std::move(description_localized);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetIconsLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      std::vector<ImageResource>>>
        icons_localized) {
  icons_localized_ = std::move(icons_localized);
  return *this;
}

base::expected<Manifest, ManifestInvariantError> Manifest::Builder::Build() {
  Manifest manifest;
  // LINT.IfChange(ManifestBuilderBuild)
  manifest.manifest_url_ = std::move(manifest_url_);
  manifest.dir_ = dir_;
  manifest.name_ = std::move(name_);
  manifest.short_name_ = std::move(short_name_);
  manifest.description_ = std::move(description_);
  manifest.id_ = std::move(id_);
  manifest.has_custom_id_ = has_custom_id_;
  manifest.start_url_ = std::move(start_url_);
  manifest.has_valid_specified_start_url_ = has_valid_specified_start_url_;
  manifest.display_ = display_;
  manifest.display_override_ = std::move(display_override_);
  manifest.orientation_ = orientation_;
  manifest.icons_ = std::move(icons_);
  manifest.screenshots_ = std::move(screenshots_);
  manifest.shortcuts_ = std::move(shortcuts_);
  manifest.share_target_ = std::move(share_target_);
  manifest.file_handlers_ = std::move(file_handlers_);
  manifest.protocol_handlers_ = std::move(protocol_handlers_);
  manifest.scope_extensions_ = std::move(scope_extensions_);
  manifest.lock_screen_ = std::move(lock_screen_);
  manifest.note_taking_ = std::move(note_taking_);
  manifest.related_applications_ = std::move(related_applications_);
  manifest.prefer_related_applications_ = prefer_related_applications_;
  manifest.migrate_from_ = std::move(migrate_from_);
  manifest.migrate_to_ = std::move(migrate_to_);
  manifest.theme_color_ = theme_color_;
  manifest.background_color_ = background_color_;
  manifest.gcm_sender_id_ = std::move(gcm_sender_id_);
  manifest.scope_ = std::move(scope_);
  manifest.update_manifest_url_ = std::move(update_manifest_url_);
  manifest.launch_handler_ = std::move(launch_handler_);
  manifest.translations_ = std::move(translations_);
  manifest.dark_theme_color_ = dark_theme_color_;
  manifest.dark_background_color_ = dark_background_color_;
  manifest.tab_strip_ = std::move(tab_strip_);
  manifest.version_ = std::move(version_);
  manifest.name_localized_ = std::move(name_localized_);
  manifest.short_name_localized_ = std::move(short_name_localized_);
  manifest.description_localized_ = std::move(description_localized_);
  manifest.icons_localized_ = std::move(icons_localized_);
  // LINT.ThenChange(
  //   :ManifestEquality,
  //   :ManifestToBuilder,
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestFields,
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestBuilderFields,
  //   //third_party/blink/common/manifest/manifest_mojom_traits.cc:ManifestStructTraitsRead
  // )

  if (auto error = manifest.ValidateInvariants()) {
    return base::unexpected(*error);
  }

  manifest.origin_ = url::Origin::Create(manifest.start_url_);
  return manifest;
}

Manifest Manifest::Builder::BuildChecked() {
  auto result = Build();
  CHECK(result.has_value()) << "Manifest invariants violated: "
                            << ManifestInvariantErrorToString(result.error());
  return std::move(*result);
}

}  // namespace blink
