// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/scoped_refptr.h"
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

static_assert(sizeof(Manifest) == sizeof(void*));
static_assert(sizeof(Manifest::Builder) == sizeof(void*));

Manifest::Data::Data() = default;
Manifest::Data::Data(const Data&) = default;
Manifest::Data::~Data() = default;

scoped_refptr<Manifest::Data> Manifest::Data::Clone() const {
  return base::WrapRefCounted(new Data(*this));
}

Manifest::Manifest(mojo::DefaultConstruct::Tag) {}
Manifest::Manifest(scoped_refptr<Data> data) {
  CHECK(data);
  CHECK(data->HasOneRef());
  data->origin = url::Origin::Create(data->start_url);
  data_ = std::move(data);
}

Manifest::Manifest(const Manifest&) = default;
Manifest& Manifest::operator=(const Manifest&) = default;
Manifest::Manifest(Manifest&&) noexcept = default;
Manifest& Manifest::operator=(Manifest&&) noexcept = default;
Manifest::~Manifest() = default;

bool Manifest::operator==(const Manifest& other) const {
  const Data& lhs = data();
  const Data& rhs = other.data();
  if (&lhs == &rhs) {
    return true;
  }
  // LINT.IfChange(ManifestEquality)
  auto AsTuple = [](const Data& d) {
    return std::tie(
        d.manifest_url, d.dir, d.name, d.short_name, d.description, d.id,
        d.has_custom_id, d.start_url, d.has_valid_specified_start_url,
        d.display, d.display_override, d.orientation, d.icons, d.screenshots,
        d.shortcuts, d.share_target, d.file_handlers, d.protocol_handlers,
        d.scope_extensions, d.lock_screen, d.note_taking,
        d.related_applications, d.prefer_related_applications, d.migrate_from,
        d.migrate_to, d.theme_color, d.background_color, d.gcm_sender_id,
        d.scope, d.update_manifest_url, d.launch_handler, d.translations,
        d.dark_theme_color, d.dark_background_color, d.tab_strip, d.version,
        d.name_localized, d.short_name_localized, d.description_localized,
        d.icons_localized);
  };
  return AsTuple(lhs) == AsTuple(rhs);
  // LINT.ThenChange(
  //   //third_party/blink/public/common/manifest/manifest.h:ManifestFields,
  //   //third_party/blink/public/mojom/manifest/manifest.mojom:ManifestFields,
  //   //third_party/blink/common/manifest/manifest_mojom_traits.cc:ManifestStructTraitsRead,
  //   //third_party/blink/common/manifest/manifest_unittest.cc:ManifestEqualityTest,
  //   //third_party/blink/common/manifest/manifest_mojom_traits_unittest.cc:ManifestMojomRoundTripFields
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
  return Builder(data().Clone());
}

// Manifest::Builder implementation

Manifest::Builder::Builder(GURL start_url, GURL id, GURL scope)
    : data_(base::MakeRefCounted<Data>()) {
  data_->id = std::move(id);
  data_->start_url = std::move(start_url);
  data_->scope = std::move(scope);
}

Manifest::Builder::Builder(GURL start_url)
    : data_(base::MakeRefCounted<Data>()) {
  data_->id = start_url.GetWithoutRef();
  data_->start_url = std::move(start_url);
  data_->scope = Manifest::GetDefaultScope(data_->start_url);
}

Manifest::Builder::Builder(scoped_refptr<Data> data) : data_(std::move(data)) {
  CHECK(data_);
  CHECK(data_->HasOneRef());
}

Manifest::Builder::~Builder() = default;

Manifest::Builder::Builder(const Builder& other)
    : data_(other.data().Clone()) {}

Manifest::Builder::Builder(Builder&&) noexcept = default;

Manifest::Builder& Manifest::Builder::operator=(const Builder& other) {
  if (this != &other) {
    data_ = other.data().Clone();
  }
  return *this;
}

Manifest::Builder& Manifest::Builder::operator=(Builder&&) noexcept = default;

Manifest::Builder& Manifest::Builder::SetManifestUrl(GURL manifest_url) {
  data().manifest_url = std::move(manifest_url);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDir(
    mojom::Manifest_TextDirection dir) {
  data().dir = dir;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetName(
    std::optional<std::u16string> name) {
  data().name = std::move(name);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShortName(
    std::optional<std::u16string> short_name) {
  data().short_name = std::move(short_name);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDescription(
    std::optional<std::u16string> description) {
  data().description = std::move(description);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetId(GURL id) {
  data().id = std::move(id);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetHasCustomId(bool has_custom_id) {
  data().has_custom_id = has_custom_id;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetStartUrl(GURL start_url) {
  data().start_url = std::move(start_url);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetHasValidSpecifiedStartUrl(
    bool has_valid_specified_start_url) {
  data().has_valid_specified_start_url = has_valid_specified_start_url;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDisplay(mojom::DisplayMode display) {
  data().display = display;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDisplayOverride(
    std::vector<DisplayOverride> display_override) {
  data().display_override = std::move(display_override);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetOrientation(
    device::mojom::ScreenOrientationLockType orientation) {
  data().orientation = orientation;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetIcons(
    std::vector<ImageResource> icons) {
  data().icons = std::move(icons);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetScreenshots(
    std::vector<Screenshot> screenshots) {
  data().screenshots = std::move(screenshots);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShortcuts(
    std::vector<ShortcutItem> shortcuts) {
  data().shortcuts = std::move(shortcuts);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShareTarget(
    std::optional<ShareTarget> share_target) {
  data().share_target = std::move(share_target);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetFileHandlers(
    std::vector<FileHandler> file_handlers) {
  data().file_handlers = std::move(file_handlers);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetProtocolHandlers(
    std::vector<ProtocolHandler> protocol_handlers) {
  data().protocol_handlers = std::move(protocol_handlers);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetScopeExtensions(
    std::vector<ScopeExtension> scope_extensions) {
  data().scope_extensions = std::move(scope_extensions);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetLockScreen(
    std::optional<LockScreen> lock_screen) {
  data().lock_screen = std::move(lock_screen);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetNoteTaking(
    std::optional<NoteTaking> note_taking) {
  data().note_taking = std::move(note_taking);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetRelatedApplications(
    std::vector<RelatedApplication> related_applications) {
  data().related_applications = std::move(related_applications);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetPreferRelatedApplications(
    bool prefer_related_applications) {
  data().prefer_related_applications = prefer_related_applications;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetMigrateFrom(
    std::vector<MigrateFrom> migrate_from) {
  data().migrate_from = std::move(migrate_from);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetMigrateTo(
    std::optional<MigrateTo> migrate_to) {
  data().migrate_to = std::move(migrate_to);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetThemeColor(
    std::optional<SkColor> theme_color) {
  data().theme_color = theme_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetBackgroundColor(
    std::optional<SkColor> background_color) {
  data().background_color = background_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetGcmSenderId(
    std::optional<std::u16string> gcm_sender_id) {
  data().gcm_sender_id = std::move(gcm_sender_id);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetScope(GURL scope) {
  data().scope = std::move(scope);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetUpdateManifestUrl(
    std::optional<GURL> update_manifest_url) {
  data().update_manifest_url = std::move(update_manifest_url);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetLaunchHandler(
    std::optional<LaunchHandler> launch_handler) {
  data().launch_handler = std::move(launch_handler);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetTranslations(
    base::flat_map<std::u16string, TranslationItem> translations) {
  data().translations = std::move(translations);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDarkThemeColor(
    std::optional<SkColor> dark_theme_color) {
  data().dark_theme_color = dark_theme_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDarkBackgroundColor(
    std::optional<SkColor> dark_background_color) {
  data().dark_background_color = dark_background_color;
  return *this;
}

Manifest::Builder& Manifest::Builder::SetTabStrip(
    std::optional<TabStrip> tab_strip) {
  data().tab_strip = std::move(tab_strip);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetVersion(
    std::optional<std::u16string> version) {
  data().version = std::move(version);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetNameLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        name_localized) {
  data().name_localized = std::move(name_localized);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetShortNameLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        short_name_localized) {
  data().short_name_localized = std::move(short_name_localized);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetDescriptionLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        description_localized) {
  data().description_localized = std::move(description_localized);
  return *this;
}

Manifest::Builder& Manifest::Builder::SetIconsLocalized(
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      std::vector<ImageResource>>>
        icons_localized) {
  data().icons_localized = std::move(icons_localized);
  return *this;
}

base::expected<Manifest, ManifestInvariantError> Manifest::Builder::Build() {
  Manifest manifest(std::move(data_));
  if (auto error = manifest.ValidateInvariants()) {
    return base::unexpected(*error);
  }
  return manifest;
}

Manifest Manifest::Builder::BuildChecked() {
  auto result = Build();
  CHECK(result.has_value()) << "Manifest invariants violated: "
                            << ManifestInvariantErrorToString(result.error());
  return std::move(*result);
}

}  // namespace blink
