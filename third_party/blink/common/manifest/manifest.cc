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
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestBuilderFields
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
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestBuilderFields
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
    std::optional<uint32_t> theme_color) {
  theme_color_ = theme_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetBackgroundColor(
    std::optional<uint32_t> background_color) {
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
    std::optional<uint32_t> dark_theme_color) {
  dark_theme_color_ = dark_theme_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDarkBackgroundColor(
    std::optional<uint32_t> dark_background_color) {
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
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestBuilderFields
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

// Temporary conversion bridges

base::expected<Manifest, ManifestInvariantError> ManifestFromMojom(
    const mojom::Manifest& mojom_manifest) {
  Manifest::Builder builder(mojom_manifest.start_url, mojom_manifest.id,
                            mojom_manifest.scope);
  builder.SetManifestUrl(mojom_manifest.manifest_url);
  builder.SetDir(mojom_manifest.dir);
  builder.SetName(mojom_manifest.name);
  builder.SetShortName(mojom_manifest.short_name);
  builder.SetDescription(mojom_manifest.description);
  builder.SetHasCustomId(mojom_manifest.has_custom_id);
  builder.SetHasValidSpecifiedStartUrl(
      mojom_manifest.has_valid_specified_start_url);
  builder.SetDisplay(mojom_manifest.display);
  builder.SetDisplayOverride(mojom_manifest.display_override);
  builder.SetOrientation(mojom_manifest.orientation);
  builder.SetIcons(mojom_manifest.icons);

  std::vector<Manifest::Screenshot> screenshots;
  screenshots.reserve(mojom_manifest.screenshots.size());
  for (const auto& s : mojom_manifest.screenshots) {
    if (s) {
      Manifest::Screenshot screenshot;
      screenshot.image = s->image;
      screenshot.form_factor = s->form_factor;
      screenshot.label = s->label;
      screenshots.push_back(std::move(screenshot));
    }
  }
  builder.SetScreenshots(std::move(screenshots));

  builder.SetShortcuts(mojom_manifest.shortcuts);
  builder.SetShareTarget(mojom_manifest.share_target);

  std::vector<Manifest::FileHandler> file_handlers;
  file_handlers.reserve(mojom_manifest.file_handlers.size());
  for (const auto& fh : mojom_manifest.file_handlers) {
    if (fh) {
      Manifest::FileHandler handler;
      handler.action = fh->action;
      handler.name = fh->name;
      handler.accept = fh->accept;
      handler.launch_type = fh->launch_type;
      file_handlers.push_back(std::move(handler));
    }
  }
  builder.SetFileHandlers(std::move(file_handlers));

  std::vector<Manifest::ProtocolHandler> protocol_handlers;
  protocol_handlers.reserve(mojom_manifest.protocol_handlers.size());
  for (const auto& ph : mojom_manifest.protocol_handlers) {
    if (ph) {
      Manifest::ProtocolHandler handler;
      handler.protocol = ph->protocol;
      handler.url = ph->url;
      protocol_handlers.push_back(std::move(handler));
    }
  }
  builder.SetProtocolHandlers(std::move(protocol_handlers));

  std::vector<Manifest::ScopeExtension> scope_extensions;
  scope_extensions.reserve(mojom_manifest.scope_extensions.size());
  for (const auto& se : mojom_manifest.scope_extensions) {
    if (se) {
      Manifest::ScopeExtension ext;
      ext.origin = se->origin;
      ext.has_origin_wildcard = se->has_origin_wildcard;
      scope_extensions.push_back(std::move(ext));
    }
  }
  builder.SetScopeExtensions(std::move(scope_extensions));

  if (mojom_manifest.lock_screen) {
    Manifest::LockScreen ls;
    ls.start_url = mojom_manifest.lock_screen->start_url;
    builder.SetLockScreen(std::move(ls));
  }

  if (mojom_manifest.note_taking) {
    Manifest::NoteTaking nt;
    nt.new_note_url = mojom_manifest.note_taking->new_note_url;
    builder.SetNoteTaking(std::move(nt));
  }

  builder.SetRelatedApplications(mojom_manifest.related_applications);
  builder.SetPreferRelatedApplications(
      mojom_manifest.prefer_related_applications);

  std::vector<Manifest::MigrateFrom> migrate_from;
  migrate_from.reserve(mojom_manifest.migrate_from.size());
  for (const auto& mf : mojom_manifest.migrate_from) {
    if (mf) {
      Manifest::MigrateFrom item;
      item.id = mf->id;
      item.install_url = mf->install_url;
      item.behavior = mf->behavior;
      migrate_from.push_back(std::move(item));
    }
  }
  builder.SetMigrateFrom(std::move(migrate_from));

  if (mojom_manifest.migrate_to) {
    Manifest::MigrateTo mt;
    mt.id = mojom_manifest.migrate_to->id;
    mt.install_url = mojom_manifest.migrate_to->install_url;
    builder.SetMigrateTo(std::move(mt));
  }

  builder.SetThemeColor(mojom_manifest.theme_color);
  builder.SetBackgroundColor(mojom_manifest.background_color);
  builder.SetGcmSenderId(mojom_manifest.gcm_sender_id);
  builder.SetUpdateManifestUrl(mojom_manifest.update_manifest_url);
  builder.SetLaunchHandler(mojom_manifest.launch_handler);
  builder.SetTranslations(mojom_manifest.translations);
  builder.SetDarkThemeColor(mojom_manifest.dark_theme_color);
  builder.SetDarkBackgroundColor(mojom_manifest.dark_background_color);
  builder.SetTabStrip(mojom_manifest.tab_strip);
  builder.SetVersion(mojom_manifest.version);

  if (mojom_manifest.name_localized) {
    absl::flat_hash_map<base::i18n::LanguageTag,
                        Manifest::ManifestLocalizedTextObject>
        map;
    for (const auto& [loc, val] : *mojom_manifest.name_localized) {
      if (val) {
        Manifest::ManifestLocalizedTextObject obj;
        obj.value = val->value;
        obj.lang = val->lang;
        obj.dir = val->dir;
        map[loc] = std::move(obj);
      }
    }
    builder.SetNameLocalized(std::move(map));
  }

  if (mojom_manifest.short_name_localized) {
    absl::flat_hash_map<base::i18n::LanguageTag,
                        Manifest::ManifestLocalizedTextObject>
        map;
    for (const auto& [loc, val] : *mojom_manifest.short_name_localized) {
      if (val) {
        Manifest::ManifestLocalizedTextObject obj;
        obj.value = val->value;
        obj.lang = val->lang;
        obj.dir = val->dir;
        map[loc] = std::move(obj);
      }
    }
    builder.SetShortNameLocalized(std::move(map));
  }

  if (mojom_manifest.description_localized) {
    absl::flat_hash_map<base::i18n::LanguageTag,
                        Manifest::ManifestLocalizedTextObject>
        map;
    for (const auto& [loc, val] : *mojom_manifest.description_localized) {
      if (val) {
        Manifest::ManifestLocalizedTextObject obj;
        obj.value = val->value;
        obj.lang = val->lang;
        obj.dir = val->dir;
        map[loc] = std::move(obj);
      }
    }
    builder.SetDescriptionLocalized(std::move(map));
  }

  if (mojom_manifest.icons_localized) {
    absl::flat_hash_map<base::i18n::LanguageTag,
                        std::vector<Manifest::ImageResource>>
        map;
    for (const auto& [loc, icon_list] : *mojom_manifest.icons_localized) {
      map[loc] = icon_list;
    }
    builder.SetIconsLocalized(std::move(map));
  }

  return std::move(builder).Build();
}

mojom::ManifestPtr ManifestToMojom(const Manifest& manifest) {
  auto mojom = mojom::Manifest::New();
  mojom->manifest_url = manifest.manifest_url();
  mojom->dir = manifest.dir();
  mojom->name = manifest.name();
  mojom->short_name = manifest.short_name();
  mojom->description = manifest.description();
  mojom->id = manifest.id();
  mojom->has_custom_id = manifest.has_custom_id();
  mojom->start_url = manifest.start_url();
  mojom->has_valid_specified_start_url =
      manifest.has_valid_specified_start_url();
  mojom->display = manifest.display();

  mojom->display_override = manifest.display_override();
  mojom->orientation = manifest.orientation();
  mojom->icons = manifest.icons();

  for (const auto& screenshot : manifest.screenshots()) {
    auto mojom_s = mojom::ManifestScreenshot::New();
    mojom_s->image = screenshot.image;
    mojom_s->form_factor = screenshot.form_factor;
    mojom_s->label = screenshot.label;
    mojom->screenshots.push_back(std::move(mojom_s));
  }

  mojom->shortcuts = manifest.shortcuts();
  mojom->share_target = manifest.share_target();

  for (const auto& fh : manifest.file_handlers()) {
    auto mojom_fh = mojom::ManifestFileHandler::New();
    mojom_fh->action = fh.action;
    mojom_fh->name = fh.name;
    mojom_fh->accept = fh.accept;
    mojom_fh->launch_type = fh.launch_type;
    mojom->file_handlers.push_back(std::move(mojom_fh));
  }

  for (const auto& ph : manifest.protocol_handlers()) {
    auto mojom_ph = mojom::ManifestProtocolHandler::New();
    mojom_ph->protocol = ph.protocol;
    mojom_ph->url = ph.url;
    mojom->protocol_handlers.push_back(std::move(mojom_ph));
  }

  for (const auto& se : manifest.scope_extensions()) {
    auto mojom_se = mojom::ManifestScopeExtension::New();
    mojom_se->origin = se.origin;
    mojom_se->has_origin_wildcard = se.has_origin_wildcard;
    mojom->scope_extensions.push_back(std::move(mojom_se));
  }

  if (manifest.lock_screen()) {
    auto mojom_ls = mojom::ManifestLockScreen::New();
    mojom_ls->start_url = manifest.lock_screen()->start_url;
    mojom->lock_screen = std::move(mojom_ls);
  }

  if (manifest.note_taking()) {
    auto mojom_nt = mojom::ManifestNoteTaking::New();
    mojom_nt->new_note_url = manifest.note_taking()->new_note_url;
    mojom->note_taking = std::move(mojom_nt);
  }

  mojom->related_applications = manifest.related_applications();
  mojom->prefer_related_applications = manifest.prefer_related_applications();

  for (const auto& mf : manifest.migrate_from()) {
    auto mojom_mf = mojom::ManifestMigrateFrom::New();
    mojom_mf->id = mf.id;
    mojom_mf->install_url = mf.install_url;
    mojom_mf->behavior = mf.behavior;
    mojom->migrate_from.push_back(std::move(mojom_mf));
  }

  if (manifest.migrate_to()) {
    auto mojom_mt = mojom::ManifestMigrateTo::New();
    mojom_mt->id = manifest.migrate_to()->id;
    mojom_mt->install_url = manifest.migrate_to()->install_url;
    mojom->migrate_to = std::move(mojom_mt);
  }

  mojom->theme_color = manifest.theme_color();
  mojom->background_color = manifest.background_color();
  mojom->gcm_sender_id = manifest.gcm_sender_id();
  mojom->scope = manifest.scope();
  mojom->update_manifest_url = manifest.update_manifest_url();

  mojom->launch_handler = manifest.launch_handler();
  mojom->translations = manifest.translations();

  mojom->dark_theme_color = manifest.dark_theme_color();
  mojom->dark_background_color = manifest.dark_background_color();

  mojom->tab_strip = manifest.tab_strip();
  mojom->version = manifest.version();

  if (manifest.name_localized()) {
    mojom->name_localized.emplace();
    for (const auto& [loc, val] : *manifest.name_localized()) {
      auto obj = mojom::ManifestLocalizedTextObject::New();
      obj->value = val.value;
      obj->dir = val.dir;
      obj->lang = val.lang;
      mojom->name_localized->emplace(loc, std::move(obj));
    }
  }

  if (manifest.short_name_localized()) {
    mojom->short_name_localized.emplace();
    for (const auto& [loc, val] : *manifest.short_name_localized()) {
      auto obj = mojom::ManifestLocalizedTextObject::New();
      obj->value = val.value;
      obj->dir = val.dir;
      obj->lang = val.lang;
      mojom->short_name_localized->emplace(loc, std::move(obj));
    }
  }

  if (manifest.description_localized()) {
    mojom->description_localized.emplace();
    for (const auto& [loc, val] : *manifest.description_localized()) {
      auto obj = mojom::ManifestLocalizedTextObject::New();
      obj->value = val.value;
      obj->dir = val.dir;
      obj->lang = val.lang;
      mojom->description_localized->emplace(loc, std::move(obj));
    }
  }

  if (manifest.icons_localized()) {
    base::flat_map<base::i18n::LanguageTag,
                   std::vector<Manifest::ImageResource>>
        map;
    for (const auto& [loc, icon_list] : *manifest.icons_localized()) {
      map[loc] = icon_list;
    }
    mojom->icons_localized = std::move(map);
  }

  return mojom;
}

}  // namespace blink
