// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_video.h"

#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutMediaTest = RenderingTest;

TEST_F(LayoutMediaTest, DisallowInlineChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-media-controls { display: inline; }
    </style>
    <video id='video'></video>
  )HTML");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowBlockChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-media-controls { display: block; }
    </style>
    <video id='video'></video>
  )HTML");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowOutOfFlowPositionedChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-media-controls { position: absolute; }
    </style>
    <video id='video'></video>
  )HTML");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

TEST_F(LayoutMediaTest, DisallowFloatingChild) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-media-controls { float: left; }
    </style>
    <video id='video'></video>
  )HTML");

  EXPECT_FALSE(GetLayoutObjectByElementId("video")->SlowFirstChild());
}

// crbug.com/1379779
TEST_F(LayoutMediaTest, BlockifyInlineFlex) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-media-controls { display: inline-flex; }
    </style>
    <video id='video'></video>
  )HTML");

  LayoutObject* child_box =
      GetLayoutObjectByElementId("video")->SlowFirstChild();
  EXPECT_FALSE(child_box->IsInline());
}

TEST_F(LayoutMediaTest, DisallowContainerBeyondMedia) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-media-controls { contain: none; }
      ::-webkit-media-controls-overlay-enclosure { position: fixed; }
    </style>
    <video controls></video>
  )HTML");
  // Pass if LayoutObject::AssertLaidOut() didn't fail.
}

}  // namespace blink
