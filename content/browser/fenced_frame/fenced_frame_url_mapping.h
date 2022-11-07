// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class FencedFrameURLMappingTestPeer;

extern const char kURNUUIDprefix[];

struct AdAuctionData {
  url::Origin interest_group_owner;
  std::string interest_group_name;
};

using ReportingMetadata = blink::mojom::FencedFrameReporting;
using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;

// Keeps a mapping of fenced frames URN:UUID and URL. Also keeps a set of
// pending mapped URN:UUIDs to support asynchronous mapping. See
// https://github.com/WICG/fenced-frame/blob/master/explainer/opaque_src.md
class CONTENT_EXPORT FencedFrameURLMapping {
 public:
  // The metadata for the shared storage runURLSelectionOperation's budget,
  // which includes the shared storage's origin and the amount of budget to
  // charge when a fenced frame that originates from the URN is navigating a top
  // frame. Before the fenced frame results in a top navigation, this
  // `SharedStorageBudgetMetadata` will be stored/associated with the URN inside
  // the `FencedFrameURLMapping`.
  struct CONTENT_EXPORT SharedStorageBudgetMetadata {
    url::Origin origin;
    mutable double budget_to_charge = 0;
  };

  // The runURLSelectionOperation's url mapping result. It contains the mapped
  // url and the `SharedStorageBudgetMetadata`.
  struct CONTENT_EXPORT SharedStorageURNMappingResult {
    GURL mapped_url;
    SharedStorageBudgetMetadata budget_metadata;
    SharedStorageReportingMap reporting_map;
    SharedStorageURNMappingResult();
    SharedStorageURNMappingResult(GURL mapped_url,
                                  SharedStorageBudgetMetadata budget_metadata,
                                  SharedStorageReportingMap reporting_map);
    ~SharedStorageURNMappingResult();
  };

 private:
  // Contains the fenced frame configuration a particular URN is mapped to.
  // This specifies how to generate a set of `FencedFrameProperties` to install
  // at navigation commit time.
  // Most properties are copied over directly from the configuration, but some
  // require some additional processing (e.g. `ad_component_configs`).
  struct MapInfo {
    MapInfo();
    explicit MapInfo(const GURL& url);
    MapInfo(const GURL& url,
            const SharedStorageBudgetMetadata& shared_storage_budget_metadata,
            const ReportingMetadata& reporting_metadata = ReportingMetadata());
    MapInfo(const MapInfo&);
    MapInfo(MapInfo&&);
    ~MapInfo();

    MapInfo& operator=(const MapInfo&);
    MapInfo& operator=(MapInfo&&);

    GURL mapped_url;

    // Extra data set if `mapped_url` is the result of a FLEDGE auction. Used
    // to fill in `AdAuctionDocumentData` for the fenced frame that navigates
    // to `mapped_url`.
    absl::optional<AdAuctionData> ad_auction_data;

    // Should be invoked whenever the URN is navigated to.
    base::RepeatingClosure on_navigate_callback;

    // Configurations for nested ad components.
    // Currently only used by FLEDGE.
    // When a fenced frame loads this configuration, these component
    // configurations will be mapped to URNs themselves, and those URNs will be
    // provided to the fenced frame for use in nested fenced frames.
    absl::optional<std::vector<MapInfo>> ad_component_configs;

    // Contains the metadata needed for shared storage budget charging. Will be
    // initialized to absl::nullopt if the associated URN is not generated from
    // shared storage. Its `budget_to_charge` can be updated to 0 when the
    // budget is charged.
    absl::optional<SharedStorageBudgetMetadata> shared_storage_budget_metadata;

    // If reporting events from fenced frames are registered, then this
    // information gets filled here.
    ReportingMetadata reporting_metadata;
  };

 public:
  // When the result of an ad auction is a main ad URL with a set of ad
  // component URLs (instead of just a single ad URL), a URN that maps to the
  // main ad URL needs to be loaded in a (parent) fenced frame, and then that
  // frame needs to have access to a new list of URNs, one for each ad component
  // URL, which it can then load in its own child fenced frames.
  //
  // To do this, the parent fenced frame needs two things, on commit:
  // 1) A list of URNs for the ad components, provided to the parent fenced
  //     frame via a Javascript API.
  // 2) Its URN to URL mapping needs to be updated to map those URNs to the ad
  //     component URLs returned by the auction.
  //
  // This class has functions that do both of these. GetURNs() returns the list
  // of URNs that need to be provided to the parent fenced frame so they are
  // accessible by the frame's scripts, and AddToMapping(), when invoked on the
  // parent fenced frame's FencedFrameURLMapping (not that of the frame that
  // actually ran the auction) adds those URNs and their corresponding URLs to
  // that mapping.
  class CONTENT_EXPORT PendingAdComponentsMap {
   public:
    PendingAdComponentsMap(const PendingAdComponentsMap&);
    PendingAdComponentsMap(PendingAdComponentsMap&&);

    ~PendingAdComponentsMap();

    PendingAdComponentsMap& operator=(const PendingAdComponentsMap&);
    PendingAdComponentsMap& operator=(PendingAdComponentsMap&&);

    // Returns the ordered list of URNs in this map.
    std::vector<GURL> GetURNs() const;

    // Exports URN to URL mappings to the passed in mapping. Generally only
    // called once per PendingAdComponentsMap, on the mapping associated with a
    // frame being navigated to a URN. Calling this twice with the
    // PendingAdComponentsMap on the same FencedFrameURLMapping will assert,
    // since it will result in adding the same URNs twice to the same mapping.
    void ExportToMapping(FencedFrameURLMapping& mapping) const;

   private:
    friend class FencedFrameURLMapping;

    // Contains an ad component URN and the configuration it maps to.
    struct AdComponent {
      GURL urn;
      MapInfo config;
    };

    explicit PendingAdComponentsMap(
        const std::vector<MapInfo>& ad_component_configs);

    std::vector<AdComponent> component_ads_;
  };

  // Contains a set of fenced frame properties. These are generated at
  // urn:uuid navigation time according to a fenced frame configuration,
  // specified by `MapInfo` above.
  // Most of these are copied from `MapInfo` directly, but some are generated
  // by another transformation, e.g.:
  // * We generate urns for the configs in `ad_component_configs` and store
  //   them in `pending_ad_components_map`.
  // * We generate a pointer to `shared_storage_budget_metadata` and store it in
  //   `shared_storage_budget_metadata`, because it should only take effect once
  //   across all fenced frames navigated to a particular configuration.
  // These `FencedFrameProperties` are stored in the fenced frame root
  // `FrameTreeNode`, and live between embedder-initiated fenced frame
  // navigations.
  struct CONTENT_EXPORT FencedFrameProperties {
    // The empty constructor is used for:
    // * pre-navigation fenced frames
    // * embedder-initiated non-opaque url navigations
    // All fields are empty, except a randomly generated partition nonce.
    FencedFrameProperties();

    // For opaque url navigations, the properties should be constructed from
    // a `MapInfo` that was previously created.
    explicit FencedFrameProperties(const MapInfo& map_info);
    FencedFrameProperties(const FencedFrameProperties&);
    FencedFrameProperties(FencedFrameProperties&&);
    ~FencedFrameProperties();

    FencedFrameProperties& operator=(const FencedFrameProperties&);
    FencedFrameProperties& operator=(FencedFrameProperties&&);

    GURL mapped_url;

    absl::optional<AdAuctionData> ad_auction_data;

    // Should be invoked when `mapped_url` is navigated to via the passed in
    // URN.
    base::RepeatingClosure on_navigate_callback;

    // urn/url mappings for ad components. These are inserted into the
    // fenced frame page's urn/url mapping when the urn navigation commits.
    absl::optional<PendingAdComponentsMap> pending_ad_components_map;

    // The pointer to the outer document page's FencedFrameURLMapping is copied
    // into the fenced frame root's FrameTreeNode. This is safe because a page
    // will outlive any NavigationRequest occurring in fenced frames in the
    // page.
    //
    // The metadata can be on fenced frame roots, and if `kAllowURNsInIframes`
    // is enabled, it can also be on any node except for the main frame node in
    // the outermost frame tree.
    absl::optional<raw_ptr<const SharedStorageBudgetMetadata>>
        shared_storage_budget_metadata;

    ReportingMetadata reporting_metadata;

    absl::optional<base::UnguessableToken> partition_nonce;
  };

  class MappingResultObserver {
   public:
    virtual ~MappingResultObserver() = default;

    // Called as soon as the URN mapping decision is made.
    //
    // On success, `properties` will be populated with the properties bound to
    // the urn:uuid.
    virtual void OnFencedFrameURLMappingComplete(
        const absl::optional<FencedFrameProperties>& properties) = 0;
  };

  FencedFrameURLMapping();
  ~FencedFrameURLMapping();
  FencedFrameURLMapping(FencedFrameURLMapping&) = delete;
  FencedFrameURLMapping& operator=(FencedFrameURLMapping&) = delete;

  // Adds a mapping for |url| to a URN:UUID that will be generated by this
  // function. Should only be invoked with a valid URL which is one of the
  // "potentially trustworthy URLs".
  // Mapping will not be added and return absl::nullopt if number of mappings
  // has reached limit. Enforcing a limit on number of mappings prevents
  // excessive memory consumption.
  // `reporting_metadata` will contain a `ReportingMetadata` that populates
  // any metadata invoked by the worklet using `RegisterAdBeacon`. See
  // https://github.com/WICG/turtledove/blob/main/Fenced_Frames_Ads_Reporting.md#registeradbeacon
  absl::optional<GURL> AddFencedFrameURL(
      const GURL& url,
      const ReportingMetadata& reporting_metadata = ReportingMetadata());

  // Move pending mapped `urn_uuid` from `pending_urn_uuid_to_url_map_` to
  // `urn_uuid_to_url_map_`. Then assign ad auction data as well as an ordered
  // list of ad component URLs, provided by a bidder running an auction, to the
  // entry associated with the `urn_uuid`. These will to be made available to
  // any fenced frame navigated to the returned URN, via the InterestGroup API.
  //
  // `on_navigate_callback` should be run on navigation to `urn_uuid`.
  //
  // See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
  void AssignFencedFrameURLAndInterestGroupInfo(
      const GURL& urn_uuid,
      const GURL& url,
      AdAuctionData auction_data,
      base::RepeatingClosure on_navigate_callback,
      std::vector<GURL> ad_component_urls,
      const ReportingMetadata& reporting_metadata = ReportingMetadata());

  // Generate a URN that is not yet mapped to a URL.
  // * For Shared Storage, it will be returned by
  // `sharedStorage.runURLSelectionOperation` before the URL selection decision
  // is made.
  // * For FLEDGE, it will be moved from `pending_urn_uuid_to_url_map_` to
  // `urn_uuid_to_url_map_` when ad auction completes. Info provided by auction
  // bidder will be assigned using `AssignFencedFrameURLAndInterestGroupInfo`.
  //
  // This method will fail and return absl::nullopt if number of
  // mappings has reached limit. Ad auction and `selectURL()` will be terminated
  // up front and an error will be reported.
  absl::optional<GURL> GeneratePendingMappedURN();

  // Register an observer for `urn_uuid`. The observer will be notified with the
  // mapping result and will be auto unregistered. If `urn_uuid` already exists
  // in `urn_uuid_to_url_map_`, or if it is not recognized at all, the observer
  // will be notified synchronously; if the mapping is pending (i.e. `urn_uuid`
  // exists in `pending_urn_uuid_to_url_map_`), the observer will be notified
  // asynchronously as soon as when the mapping decision is made.
  void ConvertFencedFrameURNToURL(const GURL& urn_uuid,
                                  MappingResultObserver* observer);

  // Explicitly unregister the observer for `urn_uuid`. This is only needed if
  // the observer is going to become invalid and the mapping is still pending.
  void RemoveObserverForURN(const GURL& urn_uuid,
                            MappingResultObserver* observer);

  // Called when the shared storage mapping decision is made for `urn_uuid`.
  // Should only be invoked on a `urn_uuid` pending to be mapped. This method
  // will trigger the observers' OnFencedFrameURLMappingComplete() method
  // associated with the `urn_uuid`, unregister those observers, and move the
  // `urn_uuid` from `pending_urn_uuid_to_url_map_` to `urn_uuid_to_url_map_`.
  void OnSharedStorageURNMappingResultDetermined(
      const GURL& urn_uuid,
      const SharedStorageURNMappingResult& mapping_result);

  // Return the `SharedStorageBudgetMetadata` associated with `urn_uuid`, or
  // nullptr if there's no metadata associated (i.e. `urn_uuid` was not
  // originated from shared storage). Precondition: `urn_uuid` exists in
  // `urn_uuid_to_url_map_`.
  //
  // This method will be called during the lifetime of a `NavigationRequest`
  // object, to associate the budget metadata to each relevant committed
  // document. A non-null returned pointer will stay valid during the
  // `FencedFrameURLMapping`'s (thus the page's) lifetime, and a page will
  // outlive any `NavigationRequest` occurring in fenced frames in the page,
  // thus it's safe for a `NavigationRequest` to store a pointer to this.
  SharedStorageBudgetMetadata* GetSharedStorageBudgetMetadata(
      const GURL& urn_uuid);

  // Modifies the true URL from a URN by replacing substrings specified in the
  // replacements map. The true URLs for any component ads associated with this
  // URN will also have substrings substituted. This function will be removed
  // once all FLEDGE auctions switch to using fenced frames.
  // TODO(crbug.com/1253118): Remove this function when we remove support for
  // showing FLEDGE ads in iframes.
  void SubstituteMappedURL(
      const GURL& urn_uuid,
      const std::vector<std::pair<std::string, std::string>>& substitutions);

 private:
  friend class FencedFrameURLMappingTestPeer;

  using UrnUuidToUrlMap = std::map<GURL, MapInfo>;

  // The maximum number of urn mappings.
  static constexpr size_t kMaxUrnMappingSize = 65536;

  // Adds an entry to `urn_uuid_to_url_map_` for `url`, generating a unique URN
  // as the key. Insertion fails if number of entries has reached the limit.
  absl::optional<UrnUuidToUrlMap::iterator> AddMappingForUrl(const GURL& url);

  bool IsMapped(const GURL& urn_uuid) const;
  bool IsPendingMapped(const GURL& urn_uuid) const;
  // Return true if number of mappings in `urn_uuid_to_url_map_` and
  // `pending_urn_uuid_to_url_map_` has reached the limit specified as
  // `kMaxUrnMappingSize`.
  bool IsFull() const;

  // The URNs that are already mapped to URLs, along with their mapping info.
  UrnUuidToUrlMap urn_uuid_to_url_map_;

  // The URNs that are not yet mapped to URLs, along with the associated
  // observers to be notified when the mapping decision is made.
  std::map<GURL, std::set<raw_ptr<MappingResultObserver>>>
      pending_urn_uuid_to_url_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_
