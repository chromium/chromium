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

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/i18n/language_tag.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
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
#include "third_party/skia/include/core/SkColor.h"
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

  // For Mojo deserialization only. Leaves `data_` null until
  // `StructTraits::Read` assigns a validated instance; all accessors `CHECK`
  // while null.
  explicit Manifest(mojo::DefaultConstruct::Tag);
  Manifest(const Manifest&);
  Manifest& operator=(const Manifest&);
  Manifest(Manifest&&) noexcept;
  Manifest& operator=(Manifest&&) noexcept;
  ~Manifest();

  bool operator==(const Manifest& other) const;

  // Starts a Builder initialized with all fields of this manifest.
  Builder ToBuilder() const;

  // The origin of this manifest (url::Origin::Create(start_url())).
  // Guaranteed never to be opaque.
  const url::Origin& GetOrigin() const LIFETIME_BOUND { return data().origin; }

  // Const accessors for all 40 fields of blink.mojom.Manifest.
  const GURL& manifest_url() const LIFETIME_BOUND {
    return data().manifest_url;
  }
  mojom::Manifest_TextDirection dir() const { return data().dir; }
  const std::optional<std::u16string>& name() const LIFETIME_BOUND {
    return data().name;
  }
  const std::optional<std::u16string>& short_name() const LIFETIME_BOUND {
    return data().short_name;
  }
  const std::optional<std::u16string>& description() const LIFETIME_BOUND {
    return data().description;
  }
  const GURL& id() const LIFETIME_BOUND { return data().id; }
  bool has_custom_id() const { return data().has_custom_id; }
  const GURL& start_url() const LIFETIME_BOUND { return data().start_url; }
  bool has_valid_specified_start_url() const {
    return data().has_valid_specified_start_url;
  }
  mojom::DisplayMode display() const { return data().display; }
  const std::vector<DisplayOverride>& display_override() const LIFETIME_BOUND {
    return data().display_override;
  }
  device::mojom::ScreenOrientationLockType orientation() const {
    return data().orientation;
  }
  const std::vector<ImageResource>& icons() const LIFETIME_BOUND {
    return data().icons;
  }
  const std::vector<Screenshot>& screenshots() const LIFETIME_BOUND {
    return data().screenshots;
  }
  const std::vector<ShortcutItem>& shortcuts() const LIFETIME_BOUND {
    return data().shortcuts;
  }
  const std::optional<ShareTarget>& share_target() const LIFETIME_BOUND {
    return data().share_target;
  }
  const std::vector<FileHandler>& file_handlers() const LIFETIME_BOUND {
    return data().file_handlers;
  }
  const std::vector<ProtocolHandler>& protocol_handlers() const LIFETIME_BOUND {
    return data().protocol_handlers;
  }
  const std::vector<ScopeExtension>& scope_extensions() const LIFETIME_BOUND {
    return data().scope_extensions;
  }
  const std::optional<LockScreen>& lock_screen() const LIFETIME_BOUND {
    return data().lock_screen;
  }
  const std::optional<NoteTaking>& note_taking() const LIFETIME_BOUND {
    return data().note_taking;
  }
  const std::vector<RelatedApplication>& related_applications() const
      LIFETIME_BOUND {
    return data().related_applications;
  }
  bool prefer_related_applications() const {
    return data().prefer_related_applications;
  }
  const std::vector<MigrateFrom>& migrate_from() const LIFETIME_BOUND {
    return data().migrate_from;
  }
  const std::optional<MigrateTo>& migrate_to() const LIFETIME_BOUND {
    return data().migrate_to;
  }
  std::optional<SkColor> theme_color() const { return data().theme_color; }
  std::optional<SkColor> background_color() const {
    return data().background_color;
  }
  const std::optional<std::u16string>& gcm_sender_id() const LIFETIME_BOUND {
    return data().gcm_sender_id;
  }
  const GURL& scope() const LIFETIME_BOUND { return data().scope; }
  const std::optional<GURL>& update_manifest_url() const LIFETIME_BOUND {
    return data().update_manifest_url;
  }
  const std::optional<LaunchHandler>& launch_handler() const LIFETIME_BOUND {
    return data().launch_handler;
  }
  const base::flat_map<std::u16string, TranslationItem>& translations() const
      LIFETIME_BOUND {
    return data().translations;
  }
  std::optional<SkColor> dark_theme_color() const {
    return data().dark_theme_color;
  }
  std::optional<SkColor> dark_background_color() const {
    return data().dark_background_color;
  }
  const std::optional<TabStrip>& tab_strip() const LIFETIME_BOUND {
    return data().tab_strip;
  }
  const std::optional<std::u16string>& version() const LIFETIME_BOUND {
    return data().version;
  }
  const std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                          ManifestLocalizedTextObject>>&
  name_localized() const LIFETIME_BOUND {
    return data().name_localized;
  }
  const std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                          ManifestLocalizedTextObject>>&
  short_name_localized() const LIFETIME_BOUND {
    return data().short_name_localized;
  }
  const std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                          ManifestLocalizedTextObject>>&
  description_localized() const LIFETIME_BOUND {
    return data().description_localized;
  }
  const std::optional<
      absl::flat_hash_map<base::i18n::LanguageTag, std::vector<ImageResource>>>&
  icons_localized() const LIFETIME_BOUND {
    return data().icons_localized;
  }

 private:
  struct Data;

  // Uses `RefCountedThreadSafe` because `Manifest` instances are shared across
  // sequences/threads (e.g., background installability/icon tasks and UI
  // callbacks). Initializes a fresh reference count when `Data::Clone()`
  // invokes the private `Data::Data(const Data&) = default` to memberwise-copy
  // all fields.
  struct RefCountedDataBase : public base::RefCountedThreadSafe<Data> {
    RefCountedDataBase() = default;

   protected:
    RefCountedDataBase(const RefCountedDataBase&)
        : base::RefCountedThreadSafe<Data>() {}
    ~RefCountedDataBase() = default;
  };

  struct Data : public RefCountedDataBase {
    Data();
    Data(Data&&) = delete;
    Data& operator=(const Data&) = delete;
    Data& operator=(Data&&) = delete;

    // Deep-copies all fields into a newly allocated Data with a fresh refcount.
    scoped_refptr<Data> Clone() const;

    // LINT.IfChange(ManifestFields)
    GURL manifest_url;
    mojom::Manifest_TextDirection dir = mojom::Manifest_TextDirection::kAuto;
    std::optional<std::u16string> name;
    std::optional<std::u16string> short_name;
    std::optional<std::u16string> description;
    GURL id;
    bool has_custom_id = false;
    GURL start_url;
    bool has_valid_specified_start_url = false;
    mojom::DisplayMode display = mojom::DisplayMode::kUndefined;
    std::vector<DisplayOverride> display_override;
    device::mojom::ScreenOrientationLockType orientation =
        device::mojom::ScreenOrientationLockType::DEFAULT;
    std::vector<ImageResource> icons;
    std::vector<Screenshot> screenshots;
    std::vector<ShortcutItem> shortcuts;
    std::optional<ShareTarget> share_target;
    std::vector<FileHandler> file_handlers;
    std::vector<ProtocolHandler> protocol_handlers;
    std::vector<ScopeExtension> scope_extensions;
    std::optional<LockScreen> lock_screen;
    std::optional<NoteTaking> note_taking;
    std::vector<RelatedApplication> related_applications;
    bool prefer_related_applications = false;
    std::vector<MigrateFrom> migrate_from;
    std::optional<MigrateTo> migrate_to;
    std::optional<SkColor> theme_color;
    std::optional<SkColor> background_color;
    std::optional<std::u16string> gcm_sender_id;
    GURL scope;
    std::optional<GURL> update_manifest_url;
    std::optional<LaunchHandler> launch_handler;
    base::flat_map<std::u16string, TranslationItem> translations;
    std::optional<SkColor> dark_theme_color;
    std::optional<SkColor> dark_background_color;
    std::optional<TabStrip> tab_strip;
    std::optional<std::u16string> version;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        name_localized;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        short_name_localized;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      ManifestLocalizedTextObject>>
        description_localized;
    std::optional<absl::flat_hash_map<base::i18n::LanguageTag,
                                      std::vector<ImageResource>>>
        icons_localized;
    // LINT.ThenChange(
    //   //third_party/blink/public/mojom/manifest/manifest.mojom:ManifestFields,
    //   //third_party/blink/common/manifest/manifest.cc:ManifestEquality,
    //   //third_party/blink/common/manifest/manifest_mojom_traits.cc:ManifestStructTraitsRead,
    //   //third_party/blink/common/manifest/manifest_unittest.cc:ManifestEqualityTest,
    //   //third_party/blink/common/manifest/manifest_mojom_traits_unittest.cc:ManifestMojomRoundTripFields
    // )

    // Cached origin created from `start_url`. Excluded from `operator==`.
    url::Origin origin;

   private:
    friend class base::RefCountedThreadSafe<Data>;
    Data(const Data&);
    ~Data();
  };

  // Enforces `HasOneRef()`, computes `origin` from `start_url`, and freezes
  // storage as `const Data`.
  explicit Manifest(scoped_refptr<Data> data);

  // `data_` is non-null on all valid `Manifest`s; it is null only on a
  // moved-from `Manifest` or an unpopulated `mojo::DefaultConstruct::Tag` shell
  // prior to `StructTraits::Read`. Routing all field access through `data()`
  // guarantees a release-mode `CHECK` crash (rather than `scoped_refptr`'s
  // `DCHECK`-only null dereference) if accessed in those states.
  const Data& data() const LIFETIME_BOUND {
    CHECK(data_);
    return *data_;
  }

  friend struct mojo::StructTraits<mojom::ManifestDataView, Manifest>;
  friend class Builder;

  // Verifies all intrinsic manifest invariants, returning the first violation
  // encountered if any.
  std::optional<ManifestInvariantError> ValidateInvariants() const;

  // Pointee `const Data` is deeply immutable and never shared with a `Builder`.
  // The `scoped_refptr` itself is non-`const` to support copy and move
  // assignment (e.g., Mojo deserialization into `*out`).
  scoped_refptr<const Data> data_;
};

// Fluent builder that validates intrinsic invariants on `Build()` /
// `BuildChecked()`. Built (`Build()` / `BuildChecked()`) or moved-from
// `Builder`s must not be used (further calls `CHECK`), while copying an unbuilt
// `Builder` deep-copies its fields.
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
  Builder& SetThemeColor(std::optional<SkColor> theme_color);
  Builder& SetBackgroundColor(std::optional<SkColor> background_color);
  Builder& SetGcmSenderId(std::optional<std::u16string> gcm_sender_id);
  Builder& SetScope(GURL scope);
  Builder& SetUpdateManifestUrl(std::optional<GURL> update_manifest_url);
  Builder& SetLaunchHandler(std::optional<LaunchHandler> launch_handler);
  Builder& SetTranslations(
      base::flat_map<std::u16string, TranslationItem> translations);
  Builder& SetDarkThemeColor(std::optional<SkColor> dark_theme_color);
  Builder& SetDarkBackgroundColor(std::optional<SkColor> dark_background_color);
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
  friend class Manifest;

  explicit Builder(scoped_refptr<Data> data);
  Data& data() LIFETIME_BOUND {
    CHECK(data_);
    CHECK(data_->HasOneRef());
    return *data_;
  }
  const Data& data() const LIFETIME_BOUND {
    CHECK(data_);
    CHECK(data_->HasOneRef());
    return *data_;
  }

  // Exclusively owned (`HasOneRef()`) until moved into `Manifest` by `Build()`.
  scoped_refptr<Data> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MANIFEST_MANIFEST_H_
