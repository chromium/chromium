// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/font_unique_name_lookup/font_unique_name_lookup_service.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "content/browser/font_unique_name_lookup/font_unique_name_lookup.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

FontUniqueNameLookupService::FontUniqueNameLookupService()
    : font_unique_name_lookup_(::content::FontUniqueNameLookup::GetInstance()) {
  DCHECK(base::FeatureList::IsEnabled(features::kFontSrcLocalMatching));
}

FontUniqueNameLookupService::~FontUniqueNameLookupService() {}

// static
void FontUniqueNameLookupService::Create(
    mojo::PendingReceiver<blink::mojom::FontUniqueNameLookup> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<FontUniqueNameLookupService>(),
                              std::move(receiver));
}

// static
scoped_refptr<base::SequencedTaskRunner>
FontUniqueNameLookupService::GetTaskRunner() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> runner(
      base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
           base::TaskPriority::USER_BLOCKING}));
  return *runner;
}

void FontUniqueNameLookupService::GetUniqueNameLookupTable(
    GetUniqueNameLookupTableCallback callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());
  if (font_unique_name_lookup_.IsValid()) {
    std::move(callback).Run(font_unique_name_lookup_.DuplicateMemoryRegion());
  } else {
    font_unique_name_lookup_.QueueShareMemoryRegionWhenReady(
        GetTaskRunner(), std::move(callback));
  }
}

void FontUniqueNameLookupService::GetUniqueNameLookupTableIfAvailable(
    GetUniqueNameLookupTableIfAvailableCallback callback) {
  DCHECK(GetTaskRunner()->RunsTasksInCurrentSequence());

  base::ReadOnlySharedMemoryRegion invalid_region;
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), false, std::move(invalid_region));

  if (!font_unique_name_lookup_.IsValid())
    return;

  std::move(callback).Run(true,
                          font_unique_name_lookup_.DuplicateMemoryRegion());
}

}  // namespace content
