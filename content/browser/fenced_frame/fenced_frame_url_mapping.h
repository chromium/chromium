// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/fenced_frame/fenced_frame_config.h"
#include "content/browser/fenced_frame/fenced_frame_reporter.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace blink {

struct AdDescriptor;
struct AdSize;

}  // namespace blink

namespace content {

class FencedFrameURLMappingTestPeer;

using SharedStorageReportingMap = base::flat_map<std::string, ::GURL>;

// Keeps a mapping of fenced frames URN:UUID and URL. Also keeps a set of
// pending mapped URN:UUIDs to support asynchronous mapping. See
// https://github.com/WICG/fenced-frame/blob/master/explainer/opaque_src.md
// TODO(crbug.com/40252330): Add methods for:
// 1. generating the pending config.
// 2. finalizing the pending config.
class CONTENT_EXPORT FencedFrameURLMapping {
 public:
  // The runURLSelectionOperation's url mapping result. It contains the mapped
  // url, the `SharedStorageBudgetMetadata`, and a FencedFrameReporter.
  struct CONTENT_EXPORT SharedStorageURNMappingResult {
    GURL mapped_url;
    SharedStorageBudgetMetadata budget_metadata;
    scoped_refptr<FencedFrameReporter> fenced_frame_reporter;
    SharedStorageURNMappingResult();
    SharedStorageURNMappingResult(
        GURL mapped_url,
        SharedStorageBudgetMetadata budget_metadata,
        scoped_refptr<FencedFrameReporter> fenced_frame_reporter);
    ~SharedStorageURNMappingResult();
  };

  class MappingResultObserver {
   public:
    virtual ~MappingResultObserver() = default;

    // Called as soon as the URN mapping decision is made.
    //
    // On success, `properties` will be populated with the properties bound to
    // the urn:uuid.
    virtual void OnFencedFrameURLMappingComplete(
        const std::optional<FencedFrameProperties>& properties) = 0;
  };

  FencedFrameURLMapping();
  ~FencedFrameURLMapping();
  FencedFrameURLMapping(FencedFrameURLMapping&) = delete;
  FencedFrameURLMapping& operator=(FencedFrameURLMapping&) = delete;

  // Imports URN to URL mappings from passed in mapping. Generally only called
  // once per PendingAdComponentsMap, on the mapping associated with a frame
  // being navigated to a URN. Calling this twice with the same
  // PendingAdComponentsMap on the same FencedFrameURLMapping will do nothing.
  void ImportPendingAdComponents(
      const std::vector<std::pair<GURL, FencedFrameConfig>>& components);

  // Move pending mapped `urn_uuid` from `pending_urn_uuid_to_url_map_` to
  // `urn_uuid_to_url_map_`. Then assign ad auction data as well as an ordered
  // list of ad component URLs, provided by a bidder running an auction, to the
  // entry associated with the `urn_uuid` and its associated
  // `FencedFrameConfig`. These will to be made available to any fenced frame
  // that gets navigated to the URN encapsulated inside the
  // `RedactedFencedFrameConfig` that is returned from this method. Either this
  // config or the internal URN inside of it is returned to script via the
  // InterestGroup API. They used to perform the fenced frame navigation.
  //
  // `on_navigate_callback` should be run on navigation to `urn_uuid`.
  //
  // See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
  blink::FencedFrame::RedactedFencedFrameConfig
  AssignFencedFrameURLAndInterestGroupInfo(
      const GURL& urn_uuid,
      std::optional<blink::AdSize> container_size,
      const blink::AdDescriptor& ad_descriptor,
      AdAuctionData auction_data,
      base::RepeatingClosure on_navigate_callback,
      std::vector<blink::AdDescriptor> ad_component_descriptors,
      scoped_refptr<FencedFrameReporter> fenced_frame_reporter = nullptr);

  // Generate a URN that is not yet mapped to a URL.
  // * For Shared Storage, it will be returned by
  // `sharedStorage.runURLSelectionOperation` before the URL selection decision
  // is made.
  // * For FLEDGE, it will be moved from `pending_urn_uuid_to_url_map_` to
  // `urn_uuid_to_url_map_` when ad auction completes. Info provided by auction
  // bidder will be assigned using `AssignFencedFrameURLAndInterestGroupInfo`.
  //
  // This method will fail and return std::nullopt if number of
  // mappings has reached limit. Ad auction and `selectURL()` will be terminated
  // up front and an error will be reported.
  std::optional<GURL> GeneratePendingMappedURN();

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
  // If the resolved URL is fenced-frame-compatible, the return value is the
  // populated fenced frame config. It is used to notify the observers in shared
  // storage worklet host manager. Tests can then obtain the populated fenced
  // frame configs from the observers.
  // Otherwise this method returns an std::nullopt.
  std::optional<FencedFrameConfig> OnSharedStorageURNMappingResultDetermined(
      const GURL& urn_uuid,
      const SharedStorageURNMappingResult& mapping_result);

  // Adds a mapping for |url| to a URN:UUID that will be generated by this
  // function. Should only be invoked with a valid URL which is one of the
  // "potentially trustworthy URLs".
  // Mapping will not be added and return std::nullopt if number of mappings
  // has reached limit. Enforcing a limit on number of mappings prevents
  // excessive memory consumption.
  // `fenced_frame_reporter` will contain a `FencedFrameReporter` to associate
  // with the created URN. It may be nullptr.
  std::optional<GURL> AddFencedFrameURLForTesting(
      const GURL& url,
      scoped_refptr<FencedFrameReporter> fenced_frame_reporter = nullptr);

  // Erases the urn_uuid_to_url_map_ and the pending_urn_uuid_to_url_map_.
  void ClearMapForTesting();

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
  SharedStorageBudgetMetadata* GetSharedStorageBudgetMetadataForTesting(
      const GURL& urn_uuid);

  // Modifies the true URL from a URN by replacing substrings specified in the
  // replacements map. The true URLs for any component ads associated with this
  // URN will also have substrings substituted. This function will be removed
  // once all FLEDGE auctions switch to using fenced frames.
  // TODO(crbug.com/40199055): Remove this function when we remove support for
  // showing FLEDGE ads in iframes.
  void SubstituteMappedURL(
      const GURL& urn_uuid,
      const std::vector<std::pair<std::string, std::string>>& substitutions);

 private:
  friend class FencedFrameURLMappingTestPeer;

  using UrnUuidToUrlMap = std::map<GURL, FencedFrameConfig>;

  // The maximum number of urn mappings.
  static constexpr size_t kMaxUrnMappingSize = 65536;

  // Adds an entry to `urn_uuid_to_url_map_` for `url`, generating a unique URN
  // as the key. Insertion fails if number of entries has reached the limit.
  std::optional<UrnUuidToUrlMap::iterator> AddMappingForUrl(const GURL& url);

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
