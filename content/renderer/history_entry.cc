// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "content/renderer/history_entry.h"

#include <algorithm>

#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebFrame;
using blink::WebHistoryItem;

namespace content {

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::AddChild(
    const WebHistoryItem& item) {
  children_.push_back(std::make_unique<HistoryNode>(entry_, item));
  return children_.back().get();
}

HistoryEntry::HistoryNode* HistoryEntry::HistoryNode::AddChild() {
  return AddChild(WebHistoryItem());
}

void HistoryEntry::HistoryNode::set_item(const WebHistoryItem& item) {
  DCHECK(!item.IsNull());
  item_ = item;
}

HistoryEntry::HistoryNode::HistoryNode(const base::WeakPtr<HistoryEntry>& entry,
                                       const WebHistoryItem& item)
    : entry_(entry) {
  if (!item.IsNull())
    set_item(item);
}

HistoryEntry::HistoryNode::~HistoryNode() {
}

std::vector<HistoryEntry::HistoryNode*> HistoryEntry::HistoryNode::children()
    const {
  std::vector<HistoryEntry::HistoryNode*> children(children_.size());
  std::transform(children_.cbegin(), children_.cend(), children.begin(),
                 [](const std::unique_ptr<HistoryEntry::HistoryNode>& item) {
                   return item.get();
                 });

  return children;
}

void HistoryEntry::HistoryNode::RemoveChildren() {
  children_.clear();
}

HistoryEntry::HistoryEntry() {
  root_.reset(
      new HistoryNode(weak_ptr_factory_.GetWeakPtr(), WebHistoryItem()));
}

HistoryEntry::~HistoryEntry() {
}

HistoryEntry::HistoryEntry(const WebHistoryItem& root) {
  root_.reset(new HistoryNode(weak_ptr_factory_.GetWeakPtr(), root));
}

}  // namespace content
