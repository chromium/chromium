// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/font_access_chooser.h"
#include "content/public/browser/font_access_context.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"
#include "url/origin.h"

namespace content {

class FontEnumerationCache;

// The ownership hierarchy for this class is:
//
// StoragePartitionImpl (1) <- (1) FontAccessManagerImpl
//
// FontAccessManagerImpl (1) <- (*) BindingContext
// FontAccessManagerImpl (1) <- (*) FontAccessChooser
//
// BindingContext (1) <- (1) GlobalRenderFrameHostId
// GlobalRenderFrameHostId (1) <-- (1) FontAccessChooser
//
// Legend:
//
// <- : owns
// <-- : corresponds to
// (N) : N is a number or *, which denotes zero or more
//
// In English:
// * There's one FontAccessManagerImpl per StoragePartitionImpl
// * Frames are bound to FontAccessManangerImpl via a BindingContext
// * The FontAccessManagerImpl owns the lifetimes of FontAccessChoosers
// * There is one FontAccessChooser for each Frame via its
// GlobalRenderFrameHostId,
//   obtained from a corresponding BindingContext
class CONTENT_EXPORT FontAccessManagerImpl
    : public blink::mojom::FontAccessManager,
      public FontAccessContext {
 public:
  // Factory method for production instances.
  static std::unique_ptr<FontAccessManagerImpl> Create();

  // Factory method with dependency injection support for testing.
  static std::unique_ptr<FontAccessManagerImpl> CreateForTesting(
      base::SequenceBound<FontEnumerationCache> font_enumeration_cache);

  // The constructor is public for internal use of std::make_unique.
  //
  // Production code should call FontAccessManagerImpl::Create(). Testing code
  // should call FontAccessManagerImpl::CreateForTesting().
  FontAccessManagerImpl(
      base::SequenceBound<FontEnumerationCache> font_enumeration_cache,
      base::PassKey<FontAccessManagerImpl>);

  FontAccessManagerImpl(const FontAccessManagerImpl&) = delete;
  FontAccessManagerImpl& operator=(const FontAccessManagerImpl&) = delete;

  ~FontAccessManagerImpl() override;

  void BindReceiver(
      url::Origin origin,
      GlobalRenderFrameHostId frame_id,
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  // blink::mojom::FontAccessManager:
  void EnumerateLocalFonts(EnumerateLocalFontsCallback callback) override;
  void ChooseLocalFonts(const std::vector<std::string>& selection,
                        ChooseLocalFontsCallback callback) override;

  // content::FontAccessContext:
  void FindAllFonts(FindAllFontsCallback callback) override;

  void SkipPrivacyChecksForTesting(bool skip) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    skip_privacy_checks_for_testing_ = skip;
  }

 private:
  struct BindingContext {
    url::Origin origin;
    GlobalRenderFrameHostId frame_id;
  };

  void DidRequestPermission(EnumerateLocalFontsCallback callback,
                            blink::mojom::PermissionStatus status);
  void DidFindAllFonts(FindAllFontsCallback callback,
                       blink::mojom::FontEnumerationStatus,
                       base::ReadOnlySharedMemoryRegion);
  void DidChooseLocalFonts(GlobalRenderFrameHostId frame_id,
                           ChooseLocalFontsCallback callback,
                           blink::mojom::FontEnumerationStatus status,
                           std::vector<blink::mojom::FontMetadataPtr> fonts);

  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<FontEnumerationCache> font_enumeration_cache_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Registered clients.
  mojo::ReceiverSet<blink::mojom::FontAccessManager, BindingContext> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const scoped_refptr<base::TaskRunner> results_task_runner_;

  bool skip_privacy_checks_for_testing_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // Here to keep the choosers alive for the user to interact with.
  std::map<GlobalRenderFrameHostId, std::unique_ptr<FontAccessChooser>>
      choosers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_
