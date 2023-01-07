// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"

#include "third_party/blink/renderer/core/inspector/inspected_frames.h"

namespace blink {

InspectorResourceContainer::InspectorResourceContainer(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames) {}

InspectorResourceContainer::~InspectorResourceContainer() = default;

void InspectorResourceContainer::Trace(Visitor* visitor) const {
  visitor->Trace(inspected_frames_);
}

void InspectorResourceContainer::DidCommitLoadForLocalFrame(LocalFrame* frame) {
  if (frame != inspected_frames_->Root())
    return;
  style_sheet_contents_.clear();
  style_element_contents_.clear();
}

void InspectorResourceContainer::StoreStyleSheetContent(const String& url,
                                                        const String& content) {
  style_sheet_contents_.Set(url, content);
}

bool InspectorResourceContainer::LoadStyleSheetContent(const String& url,
                                                       String* content) {
  if (!style_sheet_contents_.Contains(url))
    return false;
  *content = style_sheet_contents_.at(url);
  return true;
}

void InspectorResourceContainer::StoreStyleElementContent(
    DOMNodeId backend_node_id,
    const String& content) {
  style_element_contents_.Set(backend_node_id, content);
}

bool InspectorResourceContainer::LoadStyleElementContent(
    DOMNodeId backend_node_id,
    String* content) {
  if (!style_element_contents_.Contains(backend_node_id))
    return false;
  *content = style_element_contents_.at(backend_node_id);
  return true;
}

void InspectorResourceContainer::EraseStyleElementContent(
    DOMNodeId backend_node_id) {
  style_element_contents_.erase(backend_node_id);
}

}  // namespace blink
