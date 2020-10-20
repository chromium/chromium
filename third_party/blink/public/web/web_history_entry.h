// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_HISTORY_ENTRY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_HISTORY_ENTRY_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_history_item.h"

namespace blink {

class PageState;

class BLINK_EXPORT WebHistoryEntry {
 public:
  class BLINK_EXPORT HistoryNode {
   public:
    HistoryNode(const base::WeakPtr<WebHistoryEntry>& entry,
                const WebHistoryItem& item);
    HistoryNode(const HistoryNode&) = delete;
    HistoryNode& operator=(const HistoryNode&) = delete;
    ~HistoryNode();

    HistoryNode* AddChild(const WebHistoryItem& item);
    HistoryNode* AddChild();
    WebHistoryItem& item() { return item_; }
    void set_item(const WebHistoryItem& item);
    WebVector<HistoryNode*> children() const;
    void RemoveChildren();

   private:
    // When a WebHistoryEntry is destroyed, it takes all its HistoryNodes with
    // it. Use a WeakPtr to ensure that HistoryNodes don't try to illegally
    // access a dying WebHistoryEntry, or do unnecessary work when the whole
    // entry is being destroyed.
    base::WeakPtr<WebHistoryEntry> entry_;
    WebVector<std::unique_ptr<HistoryNode>> children_;
    WebHistoryItem item_;
  };

  explicit WebHistoryEntry(const WebHistoryItem& root);
  WebHistoryEntry();
  WebHistoryEntry(const WebHistoryEntry&) = delete;
  WebHistoryEntry& operator=(const WebHistoryEntry&) = delete;
  ~WebHistoryEntry();

  const WebHistoryItem& root() const { return root_->item(); }
  HistoryNode* root_history_node() const { return root_.get(); }

 private:
  std::unique_ptr<HistoryNode> root_;

  base::WeakPtrFactory<WebHistoryEntry> weak_ptr_factory_{this};
};

BLINK_EXPORT PageState HistoryEntryToPageState(WebHistoryEntry* entry);
BLINK_EXPORT PageState SingleHistoryItemToPageState(const WebHistoryItem& item);
BLINK_EXPORT std::unique_ptr<WebHistoryEntry> PageStateToHistoryEntry(
    const PageState& state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_HISTORY_ENTRY_H_
