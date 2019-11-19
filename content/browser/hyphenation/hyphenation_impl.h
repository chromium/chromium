// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HYPHENATION_HYPHENATION_IMPL_H_
#define CONTENT_BROWSER_HYPHENATION_HYPHENATION_IMPL_H_

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/hyphenation/hyphenation.mojom.h"

namespace hyphenation {

class HyphenationImpl : public blink::mojom::Hyphenation {
 public:
  HyphenationImpl();
  ~HyphenationImpl() override;

  static void Create(mojo::PendingReceiver<blink::mojom::Hyphenation>);

  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Hyphenation:
  void OpenDictionary(const std::string& locale,
                      OpenDictionaryCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HyphenationImpl);
};

}  // namespace hyphenation

#endif  // CONTENT_BROWSER_HYPHENATION_HYPHENATION_IMPL_H_
