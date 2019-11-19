// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_SERVICE_H_
#define CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_SERVICE_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/font_unique_name_lookup/font_unique_name_lookup.mojom.h"

namespace content {

class FontUniqueNameLookup;

class FontUniqueNameLookupService : public blink::mojom::FontUniqueNameLookup {
 public:
  FontUniqueNameLookupService();
  ~FontUniqueNameLookupService() override;

  static void Create(mojo::PendingReceiver<blink::mojom::FontUniqueNameLookup>);

  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  void GetUniqueNameLookupTable(
      GetUniqueNameLookupTableCallback callback) override;

  void GetUniqueNameLookupTableIfAvailable(
      GetUniqueNameLookupTableIfAvailableCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FontUniqueNameLookupService);
  ::content::FontUniqueNameLookup& font_unique_name_lookup_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_UNIQUE_NAME_LOOKUP_FONT_UNIQUE_NAME_LOOKUP_SERVICE_H_
