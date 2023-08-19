// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header defines fenced frame configs, which are objects that can be
// loaded into a fenced frame and determine its subsequent behavior. Different
// APIs like FLEDGE and sharedStorage use fenced frame configs in order to
// achieve different end-to-end privacy guarantees.
//
// Certain information stored in configs may be sensitive and therefore should
// be redacted before it is sent to a renderer process. Whether information is
// sensitive depends on the entity receiving the information, because
// "sensitivity" often refers to the possibility of joining data from multiple
// sites.
//
// As a way of specifying access controls, we represent different entities
// using the `FencedFrameEntity` enum:
// * `kEmbedder`: the renderer process that embeds the fenced frame and calls
//   the config-generating API
// * `kSameOriginContent`: the renderer process for the fenced frame content,
//   if the fenced frame content is same-origin to the config's mapped url
// * `kCrossOriginContent`: the renderer process for the fenced frame content,
//   if the fenced frame content is cross-origin to the config's mapped url
//
// When a config-generating API constructs a config, for each field in the
// config it must specify whether the field is opaque or transparent to
// the embedder and content (`VisibilityToEmbedder` and `VisibilityToContent`).
// If a field is marked as opaque for an entity, the field is redacted whenever
// the config is sent to that entity's renderer by IPC. When a field is
// redacted, the viewer can tell whether there is a value defined for that
// field, but not what the value is.
//
// Here is a summary of the information flow:
// * The embedder calls a config-generating API on the web platform, let's say
//   FLEDGE, which makes an IPC to the browser.
// * In the browser, FLEDGE generates a `FencedFrameConfig` (including the
//   visibility of each field to different entities) and stores it in
//   the Page's `FencedFrameURLMapping` data structure.
// * FLEDGE constructs a `RedactedFencedFrameConfig` from the
//   `FencedFrameConfig` and the `kEmbedder` entity. The constructor
//   automatically performs the redaction process.
//
//   TODO(crbug.com/1347953): Remove this disclaimer.
//   (Note: the following two steps aren't implemented yet, and are currently
//    accomplished with urns.)
// * FLEDGE returns the redacted config to the embedder's renderer.
//   `RedactedFencedFrameConfig` supports mojom type mappings for
//   `blink::mojom::FencedFrameConfig`.
// * Later, the embedder loads the config into a fenced frame on the web
//   platform, which sends an IPC to the browser, containing an identifier for
//   the config in the `FencedFrameURLMapping`.
//
// * The browser looks up the desired config, and creates an instance of it.
//   We call an instance of a config `FencedFrameProperties`. For most fields
//   of the config, instantiating just means copying over the original values.
//   But for some values, we do additional transformations.
// * The browser stores the `FencedFrameProperties` in the `NavigationRequest`
//   for the navigation to the fenced frame's initial src. During the navigation
//   handling, certain values inside the `FencedFrameProperties` may be used by
//   the browser, e.g. the partition nonce for network requests.
// * Upon navigation commit, the browser constructs a
//   `RedactedFencedFrameProperties` from the `FencedFrameProperties` and the
//   `kSameOriginContent` or `kCrossOriginContent` entity. The constructor
//   automatically performs the redaction process.
//
// Note: Because configs may contain nested configs (to be loaded into nested
// fenced frames), the redaction process may recurse in order to redact these
// nested configs. Nested configs are redacted for the `kEmbedder` entity,
// because the top-level fenced frame is the embedder with respect to any nested
// fenced frames.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_CONFIG_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_CONFIG_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/attribution_reporting_runtime_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FencedFrameURLMapping;

extern const char kUrnUuidPrefix[];
GURL CONTENT_EXPORT GenerateUrnUuid();

// Returns a new string based on input where the matching substrings have been
// replaced with the corresponding substitutions. This function avoids repeated
// string operations by building the output based on all substitutions, one
// substitution at a time. This effectively performs all substitutions
// simultaneously, with the earliest match in the input taking precedence.
std::string SubstituteMappedStrings(
    const std::string& input,
    const std::vector<std::pair<std::string, std::string>>& substitutions);

using AdAuctionData = blink::FencedFrame::AdAuctionData;
using DeprecatedFencedFrameMode = blink::FencedFrame::DeprecatedFencedFrameMode;
using SharedStorageBudgetMetadata =
    blink::FencedFrame::SharedStorageBudgetMetadata;

struct CONTENT_EXPORT AutomaticBeaconInfo {
  AutomaticBeaconInfo(
      const std::string& data,
      const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
      network::AttributionReportingRuntimeFeatures
          attribution_reporting_runtime_features,
      bool once);

  AutomaticBeaconInfo(const AutomaticBeaconInfo&);
  AutomaticBeaconInfo(AutomaticBeaconInfo&&);

  AutomaticBeaconInfo& operator=(const AutomaticBeaconInfo&);
  AutomaticBeaconInfo& operator=(AutomaticBeaconInfo&&);

  ~AutomaticBeaconInfo();

  std::string data;
  std::vector<blink::FencedFrame::ReportingDestination> destinations;
  // Indicates whether Attribution Reporting API related runtime features are
  // enabled and is needed for integration with Attribution Reporting API.
  network::AttributionReportingRuntimeFeatures
      attribution_reporting_runtime_features;
  // Indicates whether the automatic beacon will only be sent out for one event,
  // or if it will be sent out every time an event occurs.
  bool once;
};

// Different kinds of entities (renderers) that should receive different
// views of the information in fenced frame configs.
enum class FencedFrameEntity {
  // The document that embeds a fenced frame.
  kEmbedder,

  // The document inside a fenced frame whose origin matches the fenced frame's
  // mapped URL.
  kSameOriginContent,

  // The document inside a fenced frame whose origin doesn't match the fenced
  // frame's mapped URL.
  kCrossOriginContent,
};

// Visibility levels specify whether information should be redacted when it is
// communicated to different entities (renderers).
// * kOpaque: must be concealed from the web platform
// * kTransparent: may be exposed to the web platform
// When renderer process allocation for site isolation is sufficiently
// strict, this distinction provides security against compromised renderers,
// because the renderers receive no more information than is necessary for
// web platform-exposed features.
//
// Each entity has a different enum class, even though its values are
// (currently) identical, for stronger type safety.
enum class VisibilityToEmbedder {
  kOpaque,
  kTransparent,
};
enum class VisibilityToContent {
  kOpaque,
  kTransparent,
};

// A field in a fenced frame configuration, including:
// * an actual value for the field
// * a declaration of the visibility to the embedder
// * a declaration of the visibility to the content
template <class T>
class CONTENT_EXPORT FencedFrameProperty {
 public:
  FencedFrameProperty(T value,
                      VisibilityToEmbedder visibility_to_embedder,
                      VisibilityToContent visibility_to_content)
      : value_(std::move(value)),
        visibility_to_embedder_(visibility_to_embedder),
        visibility_to_content_(visibility_to_content) {}
  FencedFrameProperty(const FencedFrameProperty&) = default;
  FencedFrameProperty(FencedFrameProperty&&) = default;
  ~FencedFrameProperty() = default;

  FencedFrameProperty& operator=(const FencedFrameProperty&) = default;
  FencedFrameProperty& operator=(FencedFrameProperty&&) = default;

  // Get the raw value of the property, ignoring visibility to different
  // entities. Should only be used for browser-internal accesses.
  const T& GetValueIgnoringVisibility() const { return value_; }

  // Get the value of the property, redacted as necessary for the given
  // `entity`. Should be used whenever the returned information will be
  // sent to a different process or is observable from a web surface API.
  absl::optional<T> GetValueForEntity(FencedFrameEntity entity) const {
    switch (entity) {
      case FencedFrameEntity::kEmbedder: {
        if (visibility_to_embedder_ == VisibilityToEmbedder::kOpaque) {
          return absl::nullopt;
        }
        break;
      }
      case FencedFrameEntity::kCrossOriginContent: {
        // For now, content that is cross-origin to the mapped URL does not get
        // access to any of the redacted properties in the config.
        return absl::nullopt;
      }
      case FencedFrameEntity::kSameOriginContent: {
        if (visibility_to_content_ == VisibilityToContent::kOpaque) {
          return absl::nullopt;
        }
        break;
      }
    }
    return value_;
  }

 private:
  friend class content::FencedFrameURLMapping;
  friend struct FencedFrameConfig;
  friend struct FencedFrameProperties;

  T value_;
  VisibilityToEmbedder visibility_to_embedder_;
  VisibilityToContent visibility_to_content_;
};

// A collection of properties that can be loaded into a fenced frame and
// specifies its subsequent behavior. (During a navigation, they are
// transformed into a `FencedFrameProperties` object, and installed at
// navigation commit. Most properties are copied over directly from the
// configuration, but some require additional processing (e.g.
// `nested_configs`.)
//
// Config-generating APIs like FLEDGE's runAdAuction and sharedStorage's
// selectURL return urns as handles to `FencedFrameConfig`s.
// TODO(crbug.com/1417871): Turn this into a class, make its fields private,
// and have a single constructor that requires all fields to be specified.
struct CONTENT_EXPORT FencedFrameConfig {
  FencedFrameConfig();
  explicit FencedFrameConfig(const GURL& mapped_url);
  explicit FencedFrameConfig(
      const GURL& mapped_url,
      const gfx::Size& content_size,
      scoped_refptr<FencedFrameReporter> fenced_frame_reporter,
      bool is_ad_component);
  FencedFrameConfig(const GURL& urn_uuid, const GURL& url);
  FencedFrameConfig(const GURL& mapped_url,
                    scoped_refptr<FencedFrameReporter> fenced_frame_reporter,
                    bool is_ad_component);
  FencedFrameConfig(
      const GURL& urn_uuid,
      const GURL& url,
      const SharedStorageBudgetMetadata& shared_storage_budget_metadata,
      scoped_refptr<FencedFrameReporter> fenced_frame_reporter);
  FencedFrameConfig(const FencedFrameConfig&);
  FencedFrameConfig(FencedFrameConfig&&);
  ~FencedFrameConfig();

  FencedFrameConfig& operator=(const FencedFrameConfig&);
  FencedFrameConfig& operator=(FencedFrameConfig&&);

  blink::FencedFrame::RedactedFencedFrameConfig RedactFor(
      FencedFrameEntity entity) const;

  absl::optional<GURL> urn_uuid_;

  absl::optional<FencedFrameProperty<GURL>> mapped_url_;

  // The initial size of the outer container (the size that the embedder sees
  // for the fenced frame). This will only be respected if the embedder hasn't
  // explicitly declared a size for the <fencedframe> element, and will be
  // disregarded if the embedder subsequently resizes the fenced frame.
  absl::optional<FencedFrameProperty<gfx::Size>> container_size_;

  // The size of the inner frame (the size that the fenced frame sees for
  // itself).
  absl::optional<FencedFrameProperty<gfx::Size>> content_size_;

  // Whether we should use the old size freezing behavior for backwards
  // compatibility. (The old behavior is to freeze the fenced frame to its size
  // at navigation start, coerced to a list of allowed sizes. The new behavior
  // uses `container_size` and `content_size` above.)
  absl::optional<FencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;

  // Extra data set if `mapped_url` is the result of a FLEDGE auction. Used
  // to fill in `AdAuctionDocumentData` for the fenced frame that navigates
  // to `mapped_url`.
  absl::optional<FencedFrameProperty<AdAuctionData>> ad_auction_data_;

  // Should be invoked whenever the URN is navigated to.
  base::RepeatingClosure on_navigate_callback_;

  // Configurations for nested ad components.
  // Currently only used by FLEDGE.
  // When a fenced frame loads this configuration, these component
  // configurations will be mapped to URNs themselves, and those URNs will be
  // provided to the fenced frame for use in nested fenced frames.
  absl::optional<FencedFrameProperty<std::vector<FencedFrameConfig>>>
      nested_configs_;

  // Contains the metadata needed for shared storage budget charging. Will be
  // initialized to absl::nullopt if the associated URN is not generated from
  // shared storage. Its `budget_to_charge` can be updated to 0 when the
  // budget is charged.
  absl::optional<FencedFrameProperty<SharedStorageBudgetMetadata>>
      shared_storage_budget_metadata_;

  // If reporting events from fenced frames are registered, then this
  // is populated. May be nullptr, otherwise.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter_;

  // The mode for the resulting fenced frame: `kDefault` or `kOpaqueAds`.
  // TODO(crbug.com/1347953): This field is currently unused. Replace the
  // `mode` attribute of HTMLFencedFrameElement with this field in the config.
  // TODO(crbug.com/1347953): Decompose this field into flags that directly
  // control the behavior of the frame, e.g. sandbox flags. We do not want
  // mode to exist as a concept going forward.
  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;

  // Whether this is a configuration for an ad component fenced frame. Note
  // there is no corresponding field in `RedactedFencedFrameConfig`. This field
  // is only used during the construction of `FencedFrameProperties`, where it
  // is copied directly to the field of same name in `FencedFrameProperties`.
  bool is_ad_component_ = false;

  // Contains the list of permissions policy features that need to be enabled
  // for a fenced frame with this configuration to load. APIs that load fenced
  // frames, such as FLEDGE and Shared Storage, require certain features to be
  // enabled in the frame's permissions policy, but they cannot be set directly
  // by the embedder since that opens a communication channel. The API that
  // constructs the config will set this directly. These permissions will be the
  // only ones enabled in the fenced frame once it navigates.
  // See entry in spec:
  // https://wicg.github.io/fenced-frame/#fenced-frame-config-effective-enabled-permissions
  std::vector<blink::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions;
};

// Contains a set of fenced frame properties. These are generated at
// urn:uuid navigation time according to a fenced frame configuration,
// specified by `FencedFrameConfig` above.
// Most of these are copied from `FencedFrameConfig` directly, but some
// are generated by another transformation, e.g.:
// * We generate urns for the configs in `nested_configs` and store
//   them in `nested_urn_config_pairs`.
// * We generate a pointer to `shared_storage_budget_metadata` and store it in
//   `shared_storage_budget_metadata`, because it should only take effect once
//   across all fenced frames navigated to a particular configuration.
// These `FencedFrameProperties` are stored in the fenced frame root
// `FrameTreeNode`, and live between embedder-initiated fenced frame
// navigations.
// TODO(crbug.com/1417871): Turn this into a class and make its fields private.
struct CONTENT_EXPORT FencedFrameProperties {
  // The empty constructor is used for:
  // * pre-navigation fenced frames
  // * embedder-initiated non-opaque url navigations
  // All fields are empty, except a randomly generated partition nonce.
  FencedFrameProperties();

  // For opaque url navigations, the properties should be constructed from
  // a `FencedFrameConfig` that was previously created.
  explicit FencedFrameProperties(const FencedFrameConfig& map_info);
  FencedFrameProperties(const FencedFrameProperties&);
  FencedFrameProperties(FencedFrameProperties&&);
  ~FencedFrameProperties();

  FencedFrameProperties& operator=(const FencedFrameProperties&);
  FencedFrameProperties& operator=(FencedFrameProperties&&);

  blink::FencedFrame::RedactedFencedFrameProperties RedactFor(
      FencedFrameEntity entity) const;

  absl::optional<FencedFrameProperty<GURL>> mapped_url_;

  // Update the stored mapped URL to a new one given by `url`.
  // `this` must have a value for `mapped_url_` when the function is called.
  // We use this method when an embedder-initiated fenced frame root navigation
  // commits, to update the mapped URL to reflect the final destination after
  // any server-side redirects.
  void UpdateMappedURL(GURL url);

  // Stores the payload that will be sent as part of the
  // `reserved.top_navigation` automatic beacon.
  void UpdateAutomaticBeaconData(
      const std::string& event_data,
      const std::vector<blink::FencedFrame::ReportingDestination>& destinations,
      network::AttributionReportingRuntimeFeatures
          attribution_reporting_runtime_features,
      bool once);

  // Automatic beacon data is cleared out after one automatic beacon if `once`
  // was set to true when calling `setReportEventDataForAutomaticBeacons()`.
  void MaybeResetAutomaticBeaconData();

  const absl::optional<AutomaticBeaconInfo>& automatic_beacon_info() const {
    return automatic_beacon_info_;
  }

  absl::optional<FencedFrameProperty<gfx::Size>> container_size_;

  // TODO(crbug.com/1420638): The representation of size in fenced frame config
  // will need to work with the size carried with the winning bid.
  absl::optional<FencedFrameProperty<gfx::Size>> content_size_;

  absl::optional<FencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;

  absl::optional<FencedFrameProperty<AdAuctionData>> ad_auction_data_;

  // Should be invoked when `mapped_url` is navigated to via the passed in
  // URN.
  base::RepeatingClosure on_navigate_callback_;

  // urn/url mappings for ad components. These are inserted into the
  // fenced frame page's urn/url mapping when the urn navigation commits.
  absl::optional<
      FencedFrameProperty<std::vector<std::pair<GURL, FencedFrameConfig>>>>
      nested_urn_config_pairs_;

  // The pointer to the outer document page's FencedFrameURLMapping is copied
  // into the fenced frame root's FrameTreeNode. This is safe because a page
  // will outlive any NavigationRequest occurring in fenced frames in the
  // page.
  //
  // This metadata can be on fenced frame roots, and if `kAllowURNsInIframes`
  // is enabled, it can also be on any node except for the main frame node in
  // the outermost frame tree.
  absl::optional<
      FencedFrameProperty<raw_ptr<const SharedStorageBudgetMetadata>>>
      shared_storage_budget_metadata_;

  // Any context that is written by the embedder using
  // `blink::FencedFrameConfig::setSharedStorageContext`. Only readable in
  // shared storage worklets via `sharedStorage.context()`. Not copied during
  // redaction.
  absl::optional<std::u16string> embedder_shared_storage_context_;

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter_;

  absl::optional<FencedFrameProperty<base::UnguessableToken>> partition_nonce_;

  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;

  // Stores data registered by one of the documents in a FencedFrame using
  // the `Fence.setReportEventDataForAutomaticBeacons` API.
  //
  // Currently, only the `reserved.top_navigation` event exists.
  //
  // The data will be sent directly to the network, without going back to any
  // renderer process, so they are not made part of the redacted properties.
  absl::optional<AutomaticBeaconInfo> automatic_beacon_info_;

  // Whether this is an ad component fenced frame. An ad component fenced frame
  // is a nested fenced frame which loads the config from its parent fenced
  // frame's `nested_configs_`.
  // Note there is no corresponding field in `RedactedFencedFrameProperties`.
  // This flag is needed to enable automatic reportEvent beacon support for
  // ad component.
  bool is_ad_component_ = false;

  // Contains the list of permissions policy features that need to be enabled
  // for a fenced frame with this configuration to load. APIs that load fenced
  // frames, such as FLEDGE and Shared Storage, require certain features to be
  // enabled in the frame's permissions policy, but they cannot be set directly
  // by the embedder since that opens a communication channel. The API that
  // constructs the config will set this directly. These permissions will be the
  // only ones enabled in the fenced frame once it navigates.
  // See entry in spec:
  // https://wicg.github.io/fenced-frame/#fenced-frame-config-effective-enabled-permissions
  std::vector<blink::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_CONFIG_H_
