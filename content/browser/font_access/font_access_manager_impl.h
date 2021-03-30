// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_

#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/public/browser/font_access_chooser.h"
#include "content/public/browser/font_access_context.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"
#include "url/origin.h"

namespace content {

// The ownership hierarchy for this class is:
//
// StoragePartitionImpl (1) <- (1) FontAccessManagerImpl
//
// FontAccessManagerImpl (1) <- (*) BindingContext
// FontAccessManagerImpl (1) <- (*) FontAccessChooser
//
// BindingContext (1) <- (1) GlobalFrameRoutingId
// GlobalFrameRoutingId (1) <-- (1) FontAccessChooser
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
// * There is one FontAccessChooser for each Frame via its GlobalFrameRoutingId,
//   obtained from a corresponding BindingContext
class CONTENT_EXPORT FontAccessManagerImpl
    : public blink::mojom::FontAccessManager,
      public FontAccessContext {
 public:
  FontAccessManagerImpl();
  ~FontAccessManagerImpl() override;

  // Disallow copy and assign.
  FontAccessManagerImpl(const FontAccessManagerImpl&) = delete;
  FontAccessManagerImpl operator=(const FontAccessManagerImpl&) = delete;

  struct BindingContext {
    BindingContext(const url::Origin& origin, GlobalFrameRoutingId frame_id)
        : origin(origin), frame_id(frame_id) {}

    url::Origin origin;
    GlobalFrameRoutingId frame_id;
  };

  void BindReceiver(
      const BindingContext& context,
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  // blink.mojom.FontAccessManager:
  void EnumerateLocalFonts(EnumerateLocalFontsCallback callback) override;
  void ChooseLocalFonts(const std::vector<std::string>& selection,
                        ChooseLocalFontsCallback callback) override;

  // content::FontAccessContext:
  void FindAllFonts(FindAllFontsCallback callback) override;

  void SkipPrivacyChecksForTesting(bool skip) {
    skip_privacy_checks_for_testing_ = skip;
  }

 private:
  void DidRequestPermission(EnumerateLocalFontsCallback callback,
                            blink::mojom::PermissionStatus status);
  void DidFindAllFonts(FindAllFontsCallback callback,
                       blink::mojom::FontEnumerationStatus,
                       base::ReadOnlySharedMemoryRegion);
  void DidChooseLocalFonts(ChooseLocalFontsCallback callback,
                           blink::mojom::FontEnumerationStatus status,
                           std::vector<blink::mojom::FontMetadataPtr> fonts);

  // Registered clients.
  mojo::ReceiverSet<blink::mojom::FontAccessManager, BindingContext> receivers_;

  scoped_refptr<base::SequencedTaskRunner> ipc_task_runner_;
  scoped_refptr<base::TaskRunner> results_task_runner_;

  bool skip_privacy_checks_for_testing_ = false;

  // Here to keep the choosers alive for the user to interact with.
  std::map<GlobalFrameRoutingId, std::unique_ptr<FontAccessChooser>> choosers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_
