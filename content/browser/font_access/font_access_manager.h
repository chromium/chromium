// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"

namespace content {

class FontEnumerationCache;

// The ownership hierarchy for this class is:
//
// StoragePartitionImpl (1) <- (1) FontAccessManager
//
// FontAccessManager (1) <- (*) BindingContext
//
// BindingContext (1) <- (1) GlobalRenderFrameHostId
//
// Legend:
//
// <- : owns
// <-- : corresponds to
// (N) : N is a number or *, which denotes zero or more
//
// In English:
// * There's one FontAccessManager per StoragePartitionImpl
// * Frames are bound to FontAccessManager via a BindingContext.
class CONTENT_EXPORT FontAccessManager
    : public blink::mojom::FontAccessManager {
 public:
  // Factory method for production instances.
  static std::unique_ptr<FontAccessManager> Create();

  // Factory method with dependency injection support for testing.
  static std::unique_ptr<FontAccessManager> CreateForTesting(
      base::SequenceBound<FontEnumerationCache> font_enumeration_cache);

  // The constructor is public for internal use of std::make_unique.
  //
  // Production code should call FontAccessManager::Create(). Testing code
  // should call FontAccessManager::CreateForTesting().
  FontAccessManager(
      base::SequenceBound<FontEnumerationCache> font_enumeration_cache,
      base::PassKey<FontAccessManager>);

  FontAccessManager(const FontAccessManager&) = delete;
  FontAccessManager& operator=(const FontAccessManager&) = delete;

  ~FontAccessManager() override;

  void BindReceiver(
      GlobalRenderFrameHostId frame_id,
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  // blink::mojom::FontAccessManager:
  void EnumerateLocalFonts(EnumerateLocalFontsCallback callback) override;

  void SkipPrivacyChecksForTesting(bool skip) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    skip_privacy_checks_for_testing_ = skip;
  }

 private:
  struct BindingContext {
    GlobalRenderFrameHostId frame_id;
  };

  void DidRequestPermission(EnumerateLocalFontsCallback callback,
                            blink::mojom::PermissionStatus status);

  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<FontEnumerationCache> font_enumeration_cache_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Registered clients.
  mojo::ReceiverSet<blink::mojom::FontAccessManager, BindingContext> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  const scoped_refptr<base::TaskRunner> results_task_runner_;

  bool skip_privacy_checks_for_testing_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  base::WeakPtrFactory<FontAccessManager> weak_ptr_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_H_
