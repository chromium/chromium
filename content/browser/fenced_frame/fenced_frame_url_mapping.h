// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

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

    // Contains an ad component URN and the URL it maps to.
    struct AdComponent {
      GURL urn;
      GURL url;
    };

    explicit PendingAdComponentsMap(const std::vector<GURL>& ad_component_urls);

    std::vector<AdComponent> component_ads_;
  };

 private:
  // Contains the fenced frame configuration a particular URN is mapped to.
  // This specifies how to generate a set of `FencedFrameProperties` to install
  // at navigation commit time.
  // Most properties are copied over directly from the configuration, but some
  // require some additional processing (e.g. `ad_component_urls`).
  //
  // TODO(https://crbug.com/1260472): In order to only report FLEDGE results if
  // an ad is shown, InterestGroup reporting will also need to be wired up here.
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

    // Ad component URLs if `mapped_url` is the result of a FLEDGE auction. When
    // a fenced frame navigates to `mapped_url`, these will be mapped to URNs
    // themselves, and those URNs will be provided to the fenced frame.
    absl::optional<std::vector<GURL>> ad_component_urls;

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
  // Contains a set of fenced frame properties. These are generated at
  // urn:uuid navigation time according to a fenced frame configuration,
  // specified by `MapInfo` above.
  // Most of these are copied from `MapInfo` directly, but some are generated
  // by another transformation, e.g.:
  // * We generate urns for the urls in `ad_component_urls` and store them in
  // `pending_ad_components_map`.
  // * We generate a pointer to `shared_storage_budget_metadata` and store it in
  //   `shared_storage_budget_metadata`, because it should only take effect once
  //   across all fenced frames navigated to a particular configuration.
  // These `FencedFrameProperties` are stored in the fenced frame root
  // `FrameTreeNode`, and live between embedder-initiated fenced frame
  // navigations.
  struct FencedFrameProperties {
    explicit FencedFrameProperties(const MapInfo& map_info);
    FencedFrameProperties(const FencedFrameProperties&);
    FencedFrameProperties(FencedFrameProperties&&);
    ~FencedFrameProperties();

    FencedFrameProperties& operator=(const FencedFrameProperties&);
    FencedFrameProperties& operator=(FencedFrameProperties&&);

    GURL mapped_url;

    absl::optional<AdAuctionData> ad_auction_data;

    // urn/url mappings for ad components. These are inserted into the
    // fenced frame page's urn/url mapping when the urn navigation commits.
    absl::optional<PendingAdComponentsMap> pending_ad_components_map;

    // This can only be possibly set for the outermost fenced frame root,
    // because selectURL() is disallowed inside fenced frame, and the URN
    // generated outside the a fenced frame cannot be recognized from inside,
    // so a nested fenced frame can never navigate to a shared storage
    // generated URN.
    //
    // This pointer to the outermost page's FencedFrameURLMapping is copied
    // into the fenced frame root's FrameTreeNode. This is safe because a page
    // will outlive any NavigationRequest occurring in fenced frames in the
    // page.
    absl::optional<raw_ptr<const SharedStorageBudgetMetadata>>
        shared_storage_budget_metadata;

    ReportingMetadata reporting_metadata;
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

  // Assign ad auction data as well as an ordered list of ad component URLs,
  // provided by a bidder running an auction, to the existing entry associated
  // with the placeholder |urn_uuid|. These will to be made available to any
  // fenced frame navigated to the returned URN, via the InterestGroup API.
  //
  // See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
  void AssignFencedFrameURLAndInterestGroupInfo(
      const GURL& urn_uuid,
      const GURL& url,
      AdAuctionData auction_data,
      std::vector<GURL> ad_component_urls,
      const ReportingMetadata& reporting_metadata = ReportingMetadata());

  // Generate a URN that is mapped to a default constructed `MapInfo` without a
  // specified URL. This method will fail and return absl::nullopt if number of
  // mappings has reached limit. As the result, ad auction will be terminated up
  // front. If success, info provided by auction bidder will later be assigned
  // using `AssignFencedFrameURLAndInterestGroupInfo`.
  absl::optional<GURL> GeneratePlaceholderURN();

  // Generate a URN that is not yet mapped to a URL. Used by the Shared Storage
  // API to return the URN for `sharedStorage.runURLSelectionOperation` before
  // the URL selection decision is made.
  GURL GeneratePendingMappedURN();

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

  bool HasObserverForTesting(const GURL& urn_uuid,
                             MappingResultObserver* observer);

  // Returns as an out parameter the `ReportingMetadata`'s map for value
  // `"shared-storage-select-url"` associated with `urn_uuid`, or leaves the out
  // parameter unchanged if there's no shared storage reporting metadata
  // associated (i.e. `urn_uuid` did not originate from shared storage or else
  // there was no metadata passed from JavaScript). Precondition: `urn_uuid`
  // exists in `urn_uuid_to_url_map_`.
  void GetSharedStorageReportingMapForTesting(
      const GURL& urn_uuid,
      SharedStorageReportingMap* out_reporting_map);

 private:
  // Declare tests as friend of `FencedFrameURLMapping` for access to private
  // member `kMaxUrnMappingSize`.
  FRIEND_TEST_ALL_PREFIXES(FencedFrameURLMappingTest,
                           ExceedNumOfUrnMappingsLimitFailsAddURL);
  FRIEND_TEST_ALL_PREFIXES(AdAuctionServiceImplTest,
                           RunAdAuctionExceedNumOfUrnMappingsLimitFailsAuction);

  using UrnUuidToUrlMap = std::map<GURL, MapInfo>;

  // The maximum number of urn mappings.
  static constexpr size_t kMaxUrnMappingSize = 65536;

  // Adds an entry to `urn_uuid_to_url_map_` for `url`, generating a unique URN
  // as the key. Insertion fails if number of entries has reached the limit.
  absl::optional<UrnUuidToUrlMap::iterator> AddMappingForUrl(const GURL& url);

  bool IsMapped(const GURL& urn_uuid) const;
  bool IsPendingMapped(const GURL& urn_uuid) const;
  // Return true if number of mappings has reached the limit specified as
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
