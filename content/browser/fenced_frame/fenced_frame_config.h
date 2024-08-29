// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header defines fenced frame configs, which are objects that can be
// loaded into a fenced frame and determine its subsequent behavior. Different
// APIs like Protected Audience and Shared Storage use fenced frame configs in
// order to achieve different end-to-end privacy guarantees.
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
//   Protected Audience, which makes an IPC to the browser.
// * In the browser, Protected Audience generates a `FencedFrameConfig`
//   (including the visibility of each field to different entities) and stores
//   it in the Page's `FencedFrameURLMapping` data structure.
// * Protected Audience constructs a `RedactedFencedFrameConfig` from the
//   `FencedFrameConfig` and the `kEmbedder` entity. The constructor
//   automatically performs the redaction process.
//
// * Protected Audience returns the redacted config to the embedder's renderer.
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

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/fenced_frame/redacted_fenced_frame_config.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FencedFrameURLMapping;

extern const char kUrnUuidPrefix[];
GURL CONTENT_EXPORT GenerateUrnUuid();

// Used by the fenced frame properties getter. It specifies the node source
// of the fenced frame properties. TODO(crbug/40256574): kClosestAncestor is an
// artifact to support URN iframes. When URN iframes are removed, we can remove
// FencedFramePropertiesNodeSource, and all FencedFrameProperties objects will
// originate from the fenced frame root.
enum class FencedFramePropertiesNodeSource { kFrameTreeRoot, kClosestAncestor };

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
using ParentPermissionsInfo = blink::FencedFrame::ParentPermissionsInfo;

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
  std::optional<T> GetValueForEntity(FencedFrameEntity entity) const {
    switch (entity) {
      case FencedFrameEntity::kEmbedder: {
        if (visibility_to_embedder_ == VisibilityToEmbedder::kOpaque) {
          return std::nullopt;
        }
        break;
      }
      case FencedFrameEntity::kCrossOriginContent: {
        // For now, content that is cross-origin to the mapped URL does not get
        // access to any of the redacted properties in the config.
        return std::nullopt;
      }
      case FencedFrameEntity::kSameOriginContent: {
        if (visibility_to_content_ == VisibilityToContent::kOpaque) {
          return std::nullopt;
        }
        break;
      }
    }
    return value_;
  }

 private:
  friend class content::FencedFrameURLMapping;
  friend class FencedFrameConfig;
  friend class FencedFrameProperties;

  T value_;
  VisibilityToEmbedder visibility_to_embedder_;
  VisibilityToContent visibility_to_content_;
};

enum class DisableUntrustedNetworkStatus {
  kNotStarted,
  // Set when the fenced frame has called window.fence.disableUntrustedNetwork()
  // but its descendant fenced frames have not had their network access cut off
  // yet.
  kCurrentFrameTreeComplete,
  // Set after all descendant fenced frames have had network cut off.
  kCurrentAndDescendantFrameTreesComplete
};

// A collection of properties that can be loaded into a fenced frame and
// specifies its subsequent behavior. (During a navigation, they are
// transformed into a `FencedFrameProperties` object, and installed at
// navigation commit. Most properties are copied over directly from the
// configuration, but some require additional processing (e.g.
// `nested_configs`.)
//
// Config-generating APIs like Protected Audience's runAdAuction and
// sharedStorage's selectURL return urns as handles to `FencedFrameConfig`s.
// TODO(crbug.com/40257432): Use a single constructor that requires values to be
// specified for all fields, to ensure none are accidentally omitted.
class CONTENT_EXPORT FencedFrameConfig {
 public:
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

  const scoped_refptr<FencedFrameReporter>& fenced_frame_reporter() const {
    return fenced_frame_reporter_;
  }

  const std::optional<GURL>& urn_uuid() const { return urn_uuid_; }

  const std::optional<FencedFrameProperty<GURL>>& mapped_url() const {
    return mapped_url_;
  }

  // Add a permission to the FencedFrameConfig.
  // TODO(crbug.com/40233168): Refactor and expand use of test utils so there is
  // a consistent way to do this properly everywhere.
  void AddEffectiveEnabledPermissionForTesting(
      blink::mojom::PermissionsPolicyFeature feature) {
    effective_enabled_permissions_.push_back(feature);
  }

 private:
  friend class FencedFrameURLMapping;
  friend class FencedFrameProperties;
  friend class FencedFrameConfigMojomTraitsTest;
  FRIEND_TEST_ALL_PREFIXES(FencedFrameConfigMojomTraitsTest,
                           ConfigMojomTraitsTest);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameConfigMojomTraitsTest,
                           ConfigMojomTraitsModeTest);

  std::optional<GURL> urn_uuid_;

  std::optional<FencedFrameProperty<GURL>> mapped_url_;

  // The initial size of the outer container (the size that the embedder sees
  // for the fenced frame). This will only be respected if the embedder hasn't
  // explicitly declared a size for the <fencedframe> element, and will be
  // disregarded if the embedder subsequently resizes the fenced frame.
  std::optional<FencedFrameProperty<gfx::Size>> container_size_;

  // The size of the inner frame (the size that the fenced frame sees for
  // itself).
  std::optional<FencedFrameProperty<gfx::Size>> content_size_;

  // Whether we should use the old size freezing behavior for backwards
  // compatibility. (The old behavior is to freeze the fenced frame to its size
  // at navigation start, coerced to a list of allowed sizes. The new behavior
  // uses `container_size` and `content_size` above.)
  std::optional<FencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;

  // Extra data set if `mapped_url` is the result of a Protected Audience
  // auction. Used to fill in `AdAuctionDocumentData` for the fenced frame that
  // navigates to `mapped_url`.
  std::optional<FencedFrameProperty<AdAuctionData>> ad_auction_data_;

  // Should be invoked whenever the URN is navigated to.
  base::RepeatingClosure on_navigate_callback_;

  // Configurations for nested ad components.
  // Currently only used by Protected Audience.
  // When a fenced frame loads this configuration, these component
  // configurations will be mapped to URNs themselves, and those URNs will be
  // provided to the fenced frame for use in nested fenced frames.
  std::optional<FencedFrameProperty<std::vector<FencedFrameConfig>>>
      nested_configs_;

  // Contains the metadata needed for shared storage budget charging. Will be
  // initialized to std::nullopt if the associated URN is not generated from
  // shared storage. Its `budget_to_charge` can be updated to 0 when the
  // budget is charged.
  std::optional<FencedFrameProperty<SharedStorageBudgetMetadata>>
      shared_storage_budget_metadata_;

  // If reporting events from fenced frames are registered, then this
  // is populated. May be nullptr, otherwise.
  scoped_refptr<FencedFrameReporter> fenced_frame_reporter_;

  // The mode for the resulting fenced frame: `kDefault` or `kOpaqueAds`.
  // TODO(crbug.com/40233168): This field is currently unused. Replace the
  // `mode` attribute of HTMLFencedFrameElement with this field in the config.
  // TODO(crbug.com/40233168): Decompose this field into flags that directly
  // control the behavior of the frame, e.g. sandbox flags. We do not want
  // mode to exist as a concept going forward.
  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;

  // Whether information flowing into a fenced frame across the fenced boundary
  // is acceptable from a privacy standpoint. Currently, only Protected
  // Audience-created fenced frames disallow information inflow as the API has
  // protections against this communication channel. Shared Storage and web
  // platform-created configs allow arbitrary information to flow into the
  // fenced frame through URL parameters, so it's not necessary to protect
  // against other forms of information inflow.
  bool allows_information_inflow_ = false;

  // Whether this is a configuration for an ad component fenced frame. Note
  // there is no corresponding field in `RedactedFencedFrameConfig`. This field
  // is only used during the construction of `FencedFrameProperties`, where it
  // is copied directly to the field of same name in `FencedFrameProperties`.
  bool is_ad_component_ = false;

  // Contains the list of permissions policy features that need to be enabled
  // for a fenced frame with this configuration to load. APIs that load fenced
  // frames, such as Protected Audience and Shared Storage, require certain
  // features to be enabled in the frame's permissions policy, but they cannot
  // be set directly by the embedder since that opens a communication channel.
  // The API that constructs the config will set this directly. These
  // permissions will be the only ones enabled in the fenced frame once it
  // navigates. See entry in spec:
  // https://wicg.github.io/fenced-frame/#fenced-frame-config-effective-enabled-permissions
  std::vector<blink::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions_;

  // Fenced frames with flexible permissions are allowed to inherit certain
  // permissions policies from their parent. However, a fenced frame's renderer
  // process doesn't have access to its parent. Instead, we give it this
  // information through its fenced frame properties, so that it can calculate
  // inheritance. Right now, only FencedFrameConfigs created from JavaScript
  // (non-Protected Audience/Shared Storage) will have a flexible permissions
  // policy.
  std::optional<ParentPermissionsInfo> parent_permissions_info_;
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
class CONTENT_EXPORT FencedFrameProperties {
 public:
  // The empty constructor is used for:
  // * pre-navigation fenced frames
  // * embedder-initiated non-opaque url navigations
  // All fields are empty, except a randomly generated partition nonce.
  FencedFrameProperties();

  // The GURL constructor is used when loading a default config
  // FencedFrameConfig(url).
  FencedFrameProperties(const GURL& mapped_url);

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

  // Update the stored mapped URL to a new one given by `url`.
  // `this` must have a value for `mapped_url_` when the function is called.
  // We use this method when an embedder-initiated fenced frame root navigation
  // commits, to update the mapped URL to reflect the final destination after
  // any server-side redirects.
  void UpdateMappedURL(GURL url);

  // Stores information about a fenced frame's parent's permissions policy so
  // that the fenced frame's renderer process can calculate permissions
  // inheritance. This is called before the fenced frame-targeting navigation
  // commits.
  void UpdateParentParsedPermissionsPolicy(
      const blink::PermissionsPolicy* parent_policy,
      const url::Origin& parent_origin);

  const std::optional<FencedFrameProperty<GURL>>& mapped_url() const {
    return mapped_url_;
  }

  const std::optional<FencedFrameProperty<AdAuctionData>>& ad_auction_data()
      const {
    return ad_auction_data_;
  }

  const base::RepeatingClosure& on_navigate_callback() const {
    return on_navigate_callback_;
  }

  const std::optional<
      FencedFrameProperty<std::vector<std::pair<GURL, FencedFrameConfig>>>>
  nested_urn_config_pairs() const {
    return nested_urn_config_pairs_;
  }

  const std::optional<
      FencedFrameProperty<raw_ptr<const SharedStorageBudgetMetadata>>>&
  shared_storage_budget_metadata() const {
    return shared_storage_budget_metadata_;
  }

  const std::optional<std::u16string>& embedder_shared_storage_context() const {
    return embedder_shared_storage_context_;
  }

  // Used to store the shared storage context passed from the embedder
  // (navigation initiator)'s renderer into the new FencedFrameProperties.
  // TODO(crbug.com/40257432): Refactor this to be part of the
  // FencedFrameProperties constructor rather than
  // OnFencedFrameURLMappingComplete.
  void SetEmbedderSharedStorageContext(
      const std::optional<std::u16string>& embedder_shared_storage_context) {
    embedder_shared_storage_context_ = embedder_shared_storage_context;
  }

  // Stores whether the original document loaded with this config opted in to
  // cross-origin event-level reporting. That is, if the document was served
  // with the `Allow-Cross-Origin-Event-Reporting=true` response header.
  void SetAllowCrossOriginEventReporting() {
    allow_cross_origin_event_reporting_ = true;
  }

  bool allow_cross_origin_event_reporting() const {
    return allow_cross_origin_event_reporting_;
  }

  const scoped_refptr<FencedFrameReporter>& fenced_frame_reporter() const {
    return fenced_frame_reporter_;
  }

  const std::optional<FencedFrameProperty<base::UnguessableToken>>&
  partition_nonce() const {
    return partition_nonce_;
  }

  // Used for urn iframes, which should not have a separate storage/network
  // partition or access to window.fence.disableUntrustedNetwork().
  // TODO(crbug.com/40257432): Refactor this to be part of the
  // FencedFrameProperties constructor rather than
  // OnFencedFrameURLMappingComplete.
  void AdjustPropertiesForUrnIframe() {
    partition_nonce_ = std::nullopt;
    can_disable_untrusted_network_ = false;
  }

  const DeprecatedFencedFrameMode& mode() const { return mode_; }

  bool allows_information_inflow() const { return allows_information_inflow_; }

  bool is_ad_component() const { return is_ad_component_; }

  const std::vector<blink::mojom::PermissionsPolicyFeature>&
  effective_enabled_permissions() const {
    return effective_enabled_permissions_;
  }

  std::optional<ParentPermissionsInfo> parent_permissions_info() const {
    return parent_permissions_info_;
  }

  // Set the current FencedFrameProperties to have "opaque ads mode".
  // TODO(crbug.com/40233168): Refactor and expand use of test utils so there is
  // a consistent way to do this properly everywhere. Consider removing
  // arbitrary restrictions in "default mode" so that using opaque ads mode is
  // less necessary.
  void SetFencedFramePropertiesOpaqueAdsModeForTesting() {
    mode_ = blink::FencedFrame::DeprecatedFencedFrameMode::kOpaqueAds;
  }

  bool can_disable_untrusted_network() const {
    return can_disable_untrusted_network_;
  }

  bool HasDisabledNetworkForCurrentFrameTree() const {
    return disable_untrusted_network_status_ ==
               DisableUntrustedNetworkStatus::kCurrentFrameTreeComplete ||
           disable_untrusted_network_status_ ==
               DisableUntrustedNetworkStatus::
                   kCurrentAndDescendantFrameTreesComplete;
  }

  bool HasDisabledNetworkForCurrentAndDescendantFrameTrees() const {
    return disable_untrusted_network_status_ ==
           DisableUntrustedNetworkStatus::
               kCurrentAndDescendantFrameTreesComplete;
  }

  void MarkDisabledNetworkForCurrentFrameTree() {
    CHECK(can_disable_untrusted_network_);
    CHECK(
        disable_untrusted_network_status_ !=
        DisableUntrustedNetworkStatus::kCurrentAndDescendantFrameTreesComplete);
    disable_untrusted_network_status_ =
        DisableUntrustedNetworkStatus::kCurrentFrameTreeComplete;
  }

  // Safe to call multiple times (will do nothing after the first time).
  void MarkDisabledNetworkForCurrentAndDescendantFrameTrees() {
    CHECK(can_disable_untrusted_network_);
    disable_untrusted_network_status_ =
        DisableUntrustedNetworkStatus::kCurrentAndDescendantFrameTreesComplete;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(FencedFrameConfigMojomTraitsTest,
                           ConfigMojomTraitsTest);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameConfigMojomTraitsTest,
                           PropertiesHasFencedFrameReportingTest);
  FRIEND_TEST_ALL_PREFIXES(FencedFrameConfigMojomTraitsTest,
                           PropertiesCanDisableUntrustedNetworkTest);

  std::vector<std::pair<GURL, FencedFrameConfig>>
  GenerateURNConfigVectorForConfigs(
      const std::vector<FencedFrameConfig>& nested_configs);

  std::optional<FencedFrameProperty<GURL>> mapped_url_;

  std::optional<FencedFrameProperty<gfx::Size>> container_size_;

  // TODO(crbug.com/40258855): The representation of size in fenced frame config
  // will need to work with the size carried with the winning bid.
  std::optional<FencedFrameProperty<gfx::Size>> content_size_;

  std::optional<FencedFrameProperty<bool>>
      deprecated_should_freeze_initial_size_;

  std::optional<FencedFrameProperty<AdAuctionData>> ad_auction_data_;

  // Should be invoked when `mapped_url` is navigated to via the passed in
  // URN.
  base::RepeatingClosure on_navigate_callback_;

  // urn/url mappings for ad components. These are inserted into the
  // fenced frame page's urn/url mapping when the urn navigation commits.
  std::optional<
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
  std::optional<FencedFrameProperty<raw_ptr<const SharedStorageBudgetMetadata>>>
      shared_storage_budget_metadata_;

  // Any context that is written by the embedder using
  // `blink::FencedFrameConfig::setSharedStorageContext`. Only readable in
  // shared storage worklets via `sharedStorage.context()`. Not copied during
  // redaction.
  std::optional<std::u16string> embedder_shared_storage_context_;

  scoped_refptr<FencedFrameReporter> fenced_frame_reporter_;

  // The nonce that will be included in the IsolationInfo, CookiePartitionKey
  // and StorageKey for network, cookie and storage partitioning, respectively.
  // As part of IsolationInfo it is also used to identify which network requests
  // should be disallowed in the network service if the initiator fenced frame
  // tree has had its network cut off via disableUntrustedNetwork().
  std::optional<FencedFrameProperty<base::UnguessableToken>> partition_nonce_;

  DeprecatedFencedFrameMode mode_ = DeprecatedFencedFrameMode::kDefault;

  // Whether information flowing into a fenced frame across the fenced boundary
  // is acceptable from a privacy standpoint. Currently, only Protected
  // Audience-created fenced frames disallow information inflow as the API has
  // protections against this communication channel. Shared Storage and web
  // platform-created configs allow arbitrary information to flow into the
  // fenced frame through URL parameters, so it's not necessary to protect
  // against other forms of information inflow.
  bool allows_information_inflow_ = false;

  // Whether this is an ad component fenced frame. An ad component fenced frame
  // is a nested fenced frame which loads the config from its parent fenced
  // frame's `nested_configs_`.
  // Note there is no corresponding field in `RedactedFencedFrameProperties`.
  // This flag is needed to enable automatic reportEvent beacon support for
  // ad component.
  bool is_ad_component_ = false;

  // Contains the list of permissions policy features that need to be enabled
  // for a fenced frame with this configuration to load. APIs that load fenced
  // frames, such as Protected Audience and Shared Storage, require certain
  // features to be enabled in the frame's permissions policy, but they cannot
  // be set directly by the embedder since that opens a communication channel.
  // The API that constructs the config will set this directly. These
  // permissions will be the only ones enabled in the fenced frame once it
  // navigates. See entry in spec:
  // https://wicg.github.io/fenced-frame/#fenced-frame-config-effective-enabled-permissions
  std::vector<blink::mojom::PermissionsPolicyFeature>
      effective_enabled_permissions_;

  // Fenced frames with flexible permissions are allowed to inherit certain
  // permissions policies from their parent. However, a fenced frame's renderer
  // process doesn't have access to its parent. Instead, we give it this
  // information through its fenced frame properties, so that it can calculate
  // inheritance. Right now, only developer-created fenced frames (non-Protected
  // Audience/Shared Storage) will have a flexible permissions policy.
  std::optional<ParentPermissionsInfo> parent_permissions_info_;

  // Whether this config allows calls to window.fence.disableUntrustedNetwork()
  // (and then access to unpartitioned storage).
  // Currently true in all fenced frame configs, but set to false if loaded in a
  // urn iframe.
  // TODO(crbug.com/40256574): Remove this when urn iframes are removed.
  bool can_disable_untrusted_network_ = true;

  // Tracks the status of disabling untrusted network in this fenced frame. This
  // requires the fenced frame and all its descendant fenced frames to call
  // window.fence.disableUntrustedNetwork().
  DisableUntrustedNetworkStatus disable_untrusted_network_status_ =
      DisableUntrustedNetworkStatus::kNotStarted;

  // Whether the original document loaded with this config opted in to
  // cross-origin event-level reporting. That is, if the document was served
  // with the `Allow-Cross-Origin-Event-Reporting=true` response header. This is
  // the first half of the opt-in process for a cross-origin subframe to send a
  // `reportEvent()` beacon using this config's reporting metadata successfully.
  bool allow_cross_origin_event_reporting_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_CONFIG_H_
