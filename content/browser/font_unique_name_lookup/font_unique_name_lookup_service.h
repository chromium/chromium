// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_SERVICE_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_SERVICE_H_

#include "base/memory/raw_ref.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom.h"

namespace content {

class FontUniqueNameLookup;

class FontUniqueNameLookupService : public blink::mojom::FontUniqueNameLookup {
 public:
  FontUniqueNameLookupService();

  FontUniqueNameLookupService(const FontUniqueNameLookupService&) = delete;
  FontUniqueNameLookupService& operator=(const FontUniqueNameLookupService&) =
      delete;

  ~FontUniqueNameLookupService() override;

  static void Create(mojo::PendingReceiver<blink::mojom::FontUniqueNameLookup>);

  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  void GetUniqueNameLookupTable(
      GetUniqueNameLookupTableCallback callback) override;

  void GetUniqueNameLookupTableIfAvailable(
      GetUniqueNameLookupTableIfAvailableCallback callback) override;

 private:
  const raw_ref<::content::FontUniqueNameLookup> font_unique_name_lookup_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_SERVICE_H_
