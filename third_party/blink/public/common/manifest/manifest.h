// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/i18n/language_tag.h"
#include "base/types/expected.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/manifest/manifest_invariants.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest_migration_behavior.mojom-shared.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

// Validated, immutable-by-default C++ representation of `blink.mojom.Manifest`.
//
// Every `Manifest` instance guarantees its intrinsic invariants (see
// `ValidateInvariants()`): `start_url()`, `id()`, and `scope()` are valid and
// same-origin with a non-opaque `GetOrigin()`, `id()` and `scope()` have no
// URL ref, and all nested handler/icon/migration fields satisfy spec and
// security constraints. There is no "empty" state; use
// `std::optional<blink::Manifest>` when a manifest may be absent.
//
// Construct or modify instances via `Manifest::Builder`,
// `Manifest::CreateDefault()`, or `ToBuilder()`.
class BLINK_COMMON_EXPORT Manifest {
 public:
  // Structure representing an icon as per the Manifest specification, see:
  // https://w3c.github.io/manifest/#dom-imageresource
  struct BLINK_COMMON_EXPORT ImageResource {
    ImageResource();
    ImageResource(const ImageResource& other);
    ~ImageResource();

    bool operator==(const ImageResource& other) const;

    // MUST be a valid url. If an icon doesn't have a valid URL, it will not be
    // successfully parsed, thus will not be represented in the Manifest.
    GURL src;

    // Empty if the parsing failed or the field was not present. The type can be
    // any string and doesn't have to be a valid image MIME type at this point.
    // It is up to the consumer of the object to check if the type matches a
    // supported type.
    std::u16string type;

    // Empty if the parsing failed, the field was not present or empty.
    // The special value "any" is represented by gfx::Size(0, 0).
    std::vector<gfx::Size> sizes;

    // Never empty. Defaults to a vector with a single value, IconPurpose::ANY,
    // if not explicitly specified in the manifest.
    std::vector<mojom::ManifestImageResource_Purpose> purpose;
  };

  // Structure representing a localized text object as per the Manifest
  // specification, see:
  // https://www.w3.org/TR/appmanifest/#localizing-text-values
  struct BLINK_COMMON_EXPORT ManifestLocalizedTextObject {
    ManifestLocalizedTextObject() = default;
    ~ManifestLocalizedTextObject() = default;

    bool operator==(const ManifestLocalizedTextObject& other) const {
      return value == other.value && lang == other.lang && dir == other.dir;
    }

    std::u16string value;
    std::optional<std::u16string> lang;
    std::optional<blink::mojom::Manifest_TextDirection> dir;
  };

  // Structure representing a shortcut as per the Manifest specification, see:
  // https://w3c.github.io/manifest/#shortcuts-member
  struct BLINK_COMMON_EXPORT ShortcutItem {
    ShortcutItem();
    ~ShortcutItem();

    bool operator==(const ShortcutItem& other) const;

    std::u16string name;
    std::optional<std::u16string> short_name;
    std::optional<std::u16string> description;
    GURL url;
    std::vector<ImageResource> icons;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      std::vector<ImageResource>>>
        icons_localized;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        name_localized;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        short_name_localized;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        description_localized;
  };

  struct BLINK_COMMON_EXPORT FileFilter {
    bool operator==(const FileFilter& other) const;

    std::u16string name;
    std::vector<std::u16string> accept;
  };

  // Structure representing a Web Share target's query parameter keys.
  struct BLINK_COMMON_EXPORT ShareTargetParams {
    ShareTargetParams();
    ~ShareTargetParams();

    bool operator==(const ShareTargetParams& other) const;

    std::optional<std::u16string> title;
    std::optional<std::u16string> text;
    std::optional<std::u16string> url;
    std::vector<FileFilter> files;
  };

  // Structure representing how a Web Share target handles an incoming share.
  struct BLINK_COMMON_EXPORT ShareTarget {
    ShareTarget();
    ~ShareTarget();

    bool operator==(const ShareTarget& other) const;

    // The URL used for sharing. Query parameters are added to this comprised of
    // keys from |params| and values from the shared data.
    GURL action;

    // The HTTP request method for the web share target.
    blink::mojom::ManifestShareTarget_Method method;

    // The way that share data is encoded in "POST" request.
    blink::mojom::ManifestShareTarget_Enctype enctype;

    ShareTargetParams params;
  };

  // Structure representing a related application.
  struct BLINK_COMMON_EXPORT RelatedApplication {
    RelatedApplication();
    ~RelatedApplication();

    bool operator==(const RelatedApplication& other) const;

    // The platform on which the application can be found. This can be any
    // string, and is interpreted by the consumer of the object. Empty if the
    // parsing failed.
    std::optional<std::u16string> platform;

    // URL at which the application can be found. One of |url| and |id| must be
    // present. Empty if the parsing failed or the field was not present.
    GURL url;

    // An id which is used to represent the application on the platform. One of
    // |url| and |id| must be present. Empty if the parsing failed or the field
    // was not present.
    std::optional<std::u16string> id;
  };

  // This class wraps mojom::blink::ManifestLaunchHandler.
  class BLINK_COMMON_EXPORT LaunchHandler {
   public:
    using ClientMode = mojom::ManifestLaunchHandler_ClientMode;

    LaunchHandler();
    explicit LaunchHandler(std::optional<ClientMode> client_mode);

    ClientMode parsed_client_mode() const;
    bool client_mode_valid_and_specified() const;

    bool operator==(const LaunchHandler& other) const;

    bool TargetsExistingClients() const;
    bool NeverNavigateExistingClients() const;

   private:
    friend struct mojo::StructTraits<
        blink::mojom::ManifestLaunchHandlerDataView,
        ::blink::Manifest::LaunchHandler>;
    std::optional<ClientMode> client_mode_;
  };

  // Structure containing translations for the translatable manifest fields.
  struct BLINK_COMMON_EXPORT TranslationItem {
    TranslationItem();
    ~TranslationItem();

    bool operator==(const TranslationItem& other) const;

    std::optional<std::string> name;
    std::optional<std::string> short_name;
    std::optional<std::string> description;
  };

  // Parameters for the home tab customisation to the tab strip.
  struct BLINK_COMMON_EXPORT HomeTabParams {
    HomeTabParams();
    ~HomeTabParams();

    bool operator==(const HomeTabParams& other) const;

    std::vector<ImageResource> icons;
    std::vector<SafeUrlPattern> scope_patterns;
  };

  // Parameters for the new tab button customisation to the tab strip.
  struct BLINK_COMMON_EXPORT NewTabButtonParams {
    NewTabButtonParams();
    ~NewTabButtonParams();

    bool operator==(const NewTabButtonParams& other) const;

    std::optional<GURL> url;
  };

  // Structure containing customisations for the tab strip.
  struct BLINK_COMMON_EXPORT TabStrip {
    TabStrip();
    ~TabStrip();

    bool operator==(const TabStrip& other) const;

    using Visibility = blink::mojom::TabStripMemberVisibility;
    using HomeTab = std::variant<Visibility, blink::Manifest::HomeTabParams>;
    using NewTabButton = blink::Manifest::NewTabButtonParams;

    HomeTab home_tab;
    NewTabButton new_tab_button;
  };

  // Class representing an item in the 'display_override' list.
  class BLINK_COMMON_EXPORT DisplayOverride {
   public:
    static DisplayOverride Create(mojom::DisplayMode display_mode);

    // `url_patterns` are only allowed in the "unframed" display mode.
    static DisplayOverride CreateUnframed(
        std::vector<SafeUrlPattern> url_patterns = {});

    // This constructor is public so this class can be used in STL containers.
    // Prefer using the factory methods above to create instances.
    DisplayOverride();
    DisplayOverride(const DisplayOverride& other);
    DisplayOverride(DisplayOverride&& other);
    DisplayOverride& operator=(const DisplayOverride& other);
    DisplayOverride& operator=(DisplayOverride&& other);
    ~DisplayOverride();

    bool operator==(const DisplayOverride& other) const;

    // The display mode of this override.
    mojom::DisplayMode display() const { return display_; }

    // Note this may include patterns that match out-of-scope or even
    // out-of-origin URLs. Callers should verify URLs are in-scope before
    // accepting this override.
    const std::vector<SafeUrlPattern>& url_patterns() const {
      return url_patterns_;
    }

   private:
    explicit DisplayOverride(mojom::DisplayMode display_mode,
                             std::vector<SafeUrlPattern> url_patterns = {});

    mojom::DisplayMode display_ = mojom::DisplayMode::kUndefined;
    std::vector<SafeUrlPattern> url_patterns_;
  };

  // Structure representing a screenshot.
  struct BLINK_COMMON_EXPORT Screenshot {
    using FormFactor = mojom::ManifestScreenshot_FormFactor;
    bool operator==(const Screenshot&) const = default;

    ImageResource image;
    FormFactor form_factor = FormFactor::kUnknown;
    std::optional<std::u16string> label;
  };

  // Structure representing a file handler.
  struct BLINK_COMMON_EXPORT FileHandler {
    using LaunchType = mojom::ManifestFileHandler_LaunchType;
    bool operator==(const FileHandler&) const = default;

    GURL action;
    std::u16string name;
    base::flat_map<std::u16string, std::vector<std::u16string>> accept;
    LaunchType launch_type = LaunchType::kSingleClient;
  };

  // Structure representing a protocol handler.
  struct BLINK_COMMON_EXPORT ProtocolHandler {
    bool operator==(const ProtocolHandler&) const = default;

    std::u16string protocol;
    GURL url;
  };

  // Structure representing a scope extension.
  struct BLINK_COMMON_EXPORT ScopeExtension {
    bool operator==(const ScopeExtension&) const = default;

    url::Origin origin;
    bool has_origin_wildcard = false;
  };

  // Structure representing lock screen capabilities.
  struct BLINK_COMMON_EXPORT LockScreen {
    bool operator==(const LockScreen&) const = default;

    GURL start_url;
  };

  // Structure representing note taking capabilities.
  struct BLINK_COMMON_EXPORT NoteTaking {
    bool operator==(const NoteTaking&) const = default;

    GURL new_note_url;
  };

  // Structure representing a migration source for a PWA.
  struct BLINK_COMMON_EXPORT MigrateFrom {
    bool operator==(const MigrateFrom&) const = default;

    GURL id;
    std::optional<GURL> install_url;
    mojom::ManifestMigrationBehavior behavior =
        mojom::ManifestMigrationBehavior::kSuggest;
  };

  // Structure representing a migration target for a PWA.
  struct BLINK_COMMON_EXPORT MigrateTo {
    bool operator==(const MigrateTo&) const = default;

    GURL id;
    GURL install_url;
  };

  class BLINK_COMMON_EXPORT Builder;

  // The spec-defined default manifest for `document_url`, computed exactly as
  // the renderer's `ManifestManager::DefaultManifest()` does (scope from
  // `KURL::BaseAsString()` semantics, *not* `GURL::GetWithoutFilename()`).
  // Returns std::nullopt for documents that have no default manifest (invalid,
  // opaque origin, `about:`).
  static std::optional<Manifest> CreateDefault(const GURL& document_url);

  // Computes the spec-defined default scope for `document_url` using
  // `KURL::BaseAsString()` semantics.
  static GURL GetDefaultScope(const GURL& document_url);

  // For Mojo deserialization only; `StructTraits::Read` populates and
  // validates all invariants before returning the instance.
  explicit Manifest(mojo::DefaultConstruct::Tag);
  Manifest(const Manifest&);
  Manifest& operator=(const Manifest&);
  Manifest(Manifest&&) noexcept;
  Manifest& operator=(Manifest&&) noexcept;
  ~Manifest();

  bool operator==(const Manifest& other) const;

  // Starts a Builder initialized with all fields of this manifest.
  Builder ToBuilder() const;

  // The origin of this manifest (url::Origin::Create(start_url_)).
  // Guaranteed never to be opaque.
  const url::Origin& GetOrigin() const { return origin_; }

  // Const accessors for all 40 fields of blink.mojom.Manifest.
  const GURL& manifest_url() const { return manifest_url_; }
  mojom::Manifest_TextDirection dir() const { return dir_; }
  const std::optional<std::u16string>& name() const { return name_; }
  const std::optional<std::u16string>& short_name() const {
    return short_name_;
  }
  const std::optional<std::u16string>& description() const {
    return description_;
  }
  const GURL& id() const { return id_; }
  bool has_custom_id() const { return has_custom_id_; }
  const GURL& start_url() const { return start_url_; }
  bool has_valid_specified_start_url() const {
    return has_valid_specified_start_url_;
  }
  mojom::DisplayMode display() const { return display_; }
  const std::vector<DisplayOverride>& display_override() const {
    return display_override_;
  }
  device::mojom::ScreenOrientationLockType orientation() const {
    return orientation_;
  }
  const std::vector<ImageResource>& icons() const { return icons_; }
  const std::vector<Screenshot>& screenshots() const { return screenshots_; }
  const std::vector<ShortcutItem>& shortcuts() const { return shortcuts_; }
  const std::optional<ShareTarget>& share_target() const {
    return share_target_;
  }
  const std::vector<FileHandler>& file_handlers() const {
    return file_handlers_;
  }
  const std::vector<ProtocolHandler>& protocol_handlers() const {
    return protocol_handlers_;
  }
  const std::vector<ScopeExtension>& scope_extensions() const {
    return scope_extensions_;
  }
  const std::optional<LockScreen>& lock_screen() const { return lock_screen_; }
  const std::optional<NoteTaking>& note_taking() const { return note_taking_; }
  const std::vector<RelatedApplication>& related_applications() const {
    return related_applications_;
  }
  bool prefer_related_applications() const {
    return prefer_related_applications_;
  }
  const std::vector<MigrateFrom>& migrate_from() const { return migrate_from_; }
  const std::optional<MigrateTo>& migrate_to() const { return migrate_to_; }
  std::optional<uint32_t> theme_color() const { return theme_color_; }
  std::optional<uint32_t> background_color() const { return background_color_; }
  const std::optional<std::u16string>& gcm_sender_id() const {
    return gcm_sender_id_;
  }
  const GURL& scope() const { return scope_; }
  const std::optional<GURL>& update_manifest_url() const {
    return update_manifest_url_;
  }
  const std::optional<LaunchHandler>& launch_handler() const {
    return launch_handler_;
  }
  const base::flat_map<std::u16string, TranslationItem>& translations() const {
    return translations_;
  }
  std::optional<uint32_t> dark_theme_color() const { return dark_theme_color_; }
  std::optional<uint32_t> dark_background_color() const {
    return dark_background_color_;
  }
  const std::optional<TabStrip>& tab_strip() const { return tab_strip_; }
  const std::optional<std::u16string>& version() const { return version_; }
  const std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                          ManifestLocalizedTextObject>>&
  name_localized() const {
    return name_localized_;
  }
  const std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                          ManifestLocalizedTextObject>>&
  short_name_localized() const {
    return short_name_localized_;
  }
  const std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                          ManifestLocalizedTextObject>>&
  description_localized() const {
    return description_localized_;
  }
  const std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, std::vector<ImageResource>>>&
  icons_localized() const {
    return icons_localized_;
  }

 private:
  Manifest();
  friend struct mojo::StructTraits<mojom::ManifestDataView, Manifest>;
  friend class Builder;

  // Verifies all intrinsic manifest invariants, returning the first violation
  // encountered if any.
  std::optional<ManifestInvariantError> ValidateInvariants() const;

  // LINT.IfChange(ManifestFields)
  GURL manifest_url_;
  mojom::Manifest_TextDirection dir_ = mojom::Manifest_TextDirection::kAuto;
  std::optional<std::u16string> name_;
  std::optional<std::u16string> short_name_;
  std::optional<std::u16string> description_;
  GURL id_;
  bool has_custom_id_ = false;
  GURL start_url_;
  bool has_valid_specified_start_url_ = false;
  mojom::DisplayMode display_ = mojom::DisplayMode::kUndefined;
  std::vector<DisplayOverride> display_override_;
  device::mojom::ScreenOrientationLockType orientation_ =
      device::mojom::ScreenOrientationLockType::DEFAULT;
  std::vector<ImageResource> icons_;
  std::vector<Screenshot> screenshots_;
  std::vector<ShortcutItem> shortcuts_;
  std::optional<ShareTarget> share_target_;
  std::vector<FileHandler> file_handlers_;
  std::vector<ProtocolHandler> protocol_handlers_;
  std::vector<ScopeExtension> scope_extensions_;
  std::optional<LockScreen> lock_screen_;
  std::optional<NoteTaking> note_taking_;
  std::vector<RelatedApplication> related_applications_;
  bool prefer_related_applications_ = false;
  std::vector<MigrateFrom> migrate_from_;
  std::optional<MigrateTo> migrate_to_;
  std::optional<uint32_t> theme_color_;
  std::optional<uint32_t> background_color_;
  std::optional<std::u16string> gcm_sender_id_;
  GURL scope_;
  std::optional<GURL> update_manifest_url_;
  std::optional<LaunchHandler> launch_handler_;
  base::flat_map<std::u16string, TranslationItem> translations_;
  std::optional<uint32_t> dark_theme_color_;
  std::optional<uint32_t> dark_background_color_;
  std::optional<TabStrip> tab_strip_;
  std::optional<std::u16string> version_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, ManifestLocalizedTextObject>>
      name_localized_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, ManifestLocalizedTextObject>>
      short_name_localized_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, ManifestLocalizedTextObject>>
      description_localized_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, std::vector<ImageResource>>>
      icons_localized_;
  // LINT.ThenChange(
  //   :ManifestBuilderFields,
  //   //third_party/blink/public/mojom/manifest/manifest.mojom:ManifestFields,
  //   //third_party/blink/common/manifest/manifest.cc:ManifestEquality,
  //   //third_party/blink/common/manifest/manifest.cc:ManifestToBuilder,
  //   //third_party/blink/common/manifest/manifest.cc:ManifestBuilderBuild
  // )

  // Cached origin created from `start_url_`. Excluded from `operator==`.
  url::Origin origin_;
};

// Fluent builder that validates intrinsic invariants on `Build()` /
// `BuildChecked()`. Consumes the builder's fields when building.
class BLINK_COMMON_EXPORT Manifest::Builder {
 public:
  Builder(GURL start_url, GURL id, GURL scope);
  // Initializes `start_url`, defaulting `id` to `start_url.GetWithoutRef()`
  // and `scope` to `Manifest::GetDefaultScope(start_url)`.
  explicit Builder(GURL start_url);
  ~Builder();
  Builder(const Builder&);
  Builder(Builder&&) noexcept;
  Builder& operator=(const Builder&);
  Builder& operator=(Builder&&) noexcept;

  Builder& SetManifestUrl(GURL manifest_url);
  Builder& SetDir(mojom::Manifest_TextDirection dir);
  Builder& SetName(std::optional<std::u16string> name);
  Builder& SetShortName(std::optional<std::u16string> short_name);
  Builder& SetDescription(std::optional<std::u16string> description);
  Builder& SetId(GURL id);
  Builder& SetHasCustomId(bool has_custom_id);
  Builder& SetStartUrl(GURL start_url);
  Builder& SetHasValidSpecifiedStartUrl(bool has_valid_specified_start_url);
  Builder& SetDisplay(mojom::DisplayMode display);
  Builder& SetDisplayOverride(std::vector<DisplayOverride> display_override);
  Builder& SetOrientation(device::mojom::ScreenOrientationLockType orientation);
  Builder& SetIcons(std::vector<ImageResource> icons);
  Builder& SetScreenshots(std::vector<Screenshot> screenshots);
  Builder& SetShortcuts(std::vector<ShortcutItem> shortcuts);
  Builder& SetShareTarget(std::optional<ShareTarget> share_target);
  Builder& SetFileHandlers(std::vector<FileHandler> file_handlers);
  Builder& SetProtocolHandlers(std::vector<ProtocolHandler> protocol_handlers);
  Builder& SetScopeExtensions(std::vector<ScopeExtension> scope_extensions);
  Builder& SetLockScreen(std::optional<LockScreen> lock_screen);
  Builder& SetNoteTaking(std::optional<NoteTaking> note_taking);
  Builder& SetRelatedApplications(
      std::vector<RelatedApplication> related_applications);
  Builder& SetPreferRelatedApplications(bool prefer_related_applications);
  Builder& SetMigrateFrom(std::vector<MigrateFrom> migrate_from);
  Builder& SetMigrateTo(std::optional<MigrateTo> migrate_to);
  Builder& SetThemeColor(std::optional<uint32_t> theme_color);
  Builder& SetBackgroundColor(std::optional<uint32_t> background_color);
  Builder& SetGcmSenderId(std::optional<std::u16string> gcm_sender_id);
  Builder& SetScope(GURL scope);
  Builder& SetUpdateManifestUrl(std::optional<GURL> update_manifest_url);
  Builder& SetLaunchHandler(std::optional<LaunchHandler> launch_handler);
  Builder& SetTranslations(
      base::flat_map<std::u16string, TranslationItem> translations);
  Builder& SetDarkThemeColor(std::optional<uint32_t> dark_theme_color);
  Builder& SetDarkBackgroundColor(
      std::optional<uint32_t> dark_background_color);
  Builder& SetTabStrip(std::optional<TabStrip> tab_strip);
  Builder& SetVersion(std::optional<std::u16string> version);
  Builder& SetNameLocalized(
      std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                        ManifestLocalizedTextObject>>
          name_localized);
  Builder& SetShortNameLocalized(
      std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                        ManifestLocalizedTextObject>>
          short_name_localized);
  Builder& SetDescriptionLocalized(
      std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                        ManifestLocalizedTextObject>>
          description_localized);
  Builder& SetIconsLocalized(
      std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                        std::vector<ImageResource>>>
          icons_localized);

  // Runs the intrinsic checks. The primary way to get a Manifest outside Mojo.
  base::expected<Manifest, ManifestInvariantError> Build();

  // CHECKs that the intrinsic checks pass. Only for inputs that are valid by
  // construction.
  Manifest BuildChecked();

 private:
  // LINT.IfChange(ManifestBuilderFields)
  GURL manifest_url_;
  mojom::Manifest_TextDirection dir_ = mojom::Manifest_TextDirection::kAuto;
  std::optional<std::u16string> name_;
  std::optional<std::u16string> short_name_;
  std::optional<std::u16string> description_;
  GURL id_;
  bool has_custom_id_ = false;
  GURL start_url_;
  bool has_valid_specified_start_url_ = false;
  mojom::DisplayMode display_ = mojom::DisplayMode::kUndefined;
  std::vector<DisplayOverride> display_override_;
  device::mojom::ScreenOrientationLockType orientation_ =
      device::mojom::ScreenOrientationLockType::DEFAULT;
  std::vector<ImageResource> icons_;
  std::vector<Screenshot> screenshots_;
  std::vector<ShortcutItem> shortcuts_;
  std::optional<ShareTarget> share_target_;
  std::vector<FileHandler> file_handlers_;
  std::vector<ProtocolHandler> protocol_handlers_;
  std::vector<ScopeExtension> scope_extensions_;
  std::optional<LockScreen> lock_screen_;
  std::optional<NoteTaking> note_taking_;
  std::vector<RelatedApplication> related_applications_;
  bool prefer_related_applications_ = false;
  std::vector<MigrateFrom> migrate_from_;
  std::optional<MigrateTo> migrate_to_;
  std::optional<uint32_t> theme_color_;
  std::optional<uint32_t> background_color_;
  std::optional<std::u16string> gcm_sender_id_;
  GURL scope_;
  std::optional<GURL> update_manifest_url_;
  std::optional<LaunchHandler> launch_handler_;
  base::flat_map<std::u16string, TranslationItem> translations_;
  std::optional<uint32_t> dark_theme_color_;
  std::optional<uint32_t> dark_background_color_;
  std::optional<TabStrip> tab_strip_;
  std::optional<std::u16string> version_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, ManifestLocalizedTextObject>>
      name_localized_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, ManifestLocalizedTextObject>>
      short_name_localized_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, ManifestLocalizedTextObject>>
      description_localized_;
  std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, std::vector<ImageResource>>>
      icons_localized_;
  // LINT.ThenChange(
  //   :ManifestFields,
  //   //third_party/blink/public/mojom/manifest/manifest.mojom:ManifestFields,
  //   //third_party/blink/common/manifest/manifest.cc:ManifestEquality,
  //   //third_party/blink/common/manifest/manifest.cc:ManifestToBuilder,
  //   //third_party/blink/common/manifest/manifest.cc:ManifestBuilderBuild
  // )
};

// Temporary conversion bridges between `blink::Manifest` and
// `blink::mojom::Manifest`, used while incrementally migrating consumers before
// activating the `blink.mojom.Manifest` -> `blink::Manifest` Mojo `cpp_typemaps`
// (https://crbug.com/485890278).
BLINK_COMMON_EXPORT base::expected<Manifest, ManifestInvariantError>
ManifestFromMojom(const mojom::Manifest& mojom_manifest);

BLINK_COMMON_EXPORT mojom::ManifestPtr ManifestToMojom(
    const Manifest& manifest);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_
