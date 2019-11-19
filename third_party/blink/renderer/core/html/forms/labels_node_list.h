/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LABELS_NODE_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LABELS_NODE_LIST_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/live_node_list.h"

namespace blink {

class LabelsNodeList final : public LiveNodeList {
 public:
  explicit LabelsNodeList(ContainerNode&);
  LabelsNodeList(ContainerNode& owner_node, CollectionType type);
  ~LabelsNodeList() override;

 protected:
  bool ElementMatches(const Element&) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_LABELS_NODE_LIST_H_
