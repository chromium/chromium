// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"

namespace blink {

class VisualRectPerfTest : public RenderingTest {
 public:
  void RunPerfTest(unsigned iteration_count,
                   const LayoutBoxModelObject& target,
                   const LayoutBoxModelObject& ancestor,
                   const PhysicalRect& rect);
};

void VisualRectPerfTest::RunPerfTest(unsigned iteration_count,
                                     const LayoutBoxModelObject& object,
                                     const LayoutBoxModelObject& ancestor,
                                     const PhysicalRect& rect) {
  PhysicalRect test_rect(rect);
  base::TimeTicks start = base::TimeTicks::Now();
  for (unsigned count = 0; count < iteration_count; count++) {
    object.MapToVisualRectInAncestorSpace(&ancestor, test_rect);
  }
  LOG(ERROR) << "  Time to run MapToVisualRectInAncestorSpace: "
             << (base::TimeTicks::Now() - start).InMilliseconds() << "ms";

  start = base::TimeTicks::Now();
  size_t total_cache_bytes = 0;
  for (unsigned count = 0; count < iteration_count; count++) {
    object.MapToVisualRectInAncestorSpace(&ancestor, test_rect,
                                          kUseGeometryMapper);
    if (count == 0) {
      total_cache_bytes = object.FirstFragment()
                              .LocalBorderBoxProperties()
                              .CacheMemoryUsageInBytes();
    }
    GeometryMapper::ClearCache();
  }

  LOG(ERROR)
      << "  Time to run MapToVisualRectInAncestorSpace w/GeometryMapper: "

      << (base::TimeTicks::Now() - start).InMilliseconds() << "ms";
  LOG(ERROR) << "    GeometryMapper cache storage size: " << total_cache_bytes
             << " bytes";
}

TEST_F(VisualRectPerfTest, GeometryMapper) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin:0;
      }
      .paintLayer {
        position: relative;
      }
      .transform {
        transform: translateX(1px);
      }
      .target {
        position: relative;
        width: 100px;
        height: 100px;
      }

      </style>
    <div id=singleDiv class=target></div>
    <div>
      <div>
        <div>
          <div>
            <div>
              <div>
                <div>
                  <div>
                    <div>
                      <div>
                        <div id=nestedDiv class=target></div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div class=paintLayer>
      <div class=paintLayer>
        <div class=paintLayer>
          <div class=paintLayer>
            <div class=paintLayer>
              <div class=paintLayer>
                <div class=paintLayer>
                  <div class=paintLayer
                    <div class=paintLayer>
                      <div class=paintLayer>
                        <div id=nestedPaintLayers class=target></div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>

    <div class=transform>
      <div class=transform>
        <div class=transform>
          <div class=transform>
            <div class=transform>
              <div class=transform>
                <div class=transform>
                  <div class=transform
                    <div class=transform>
                      <div class=transform>
                        <div id=nestedTransform class=target></div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutView* view = GetDocument().View()->GetLayoutView();
  PhysicalRect rect(0, 0, 100, 100);

  unsigned kIterationCount = 1000000;
  LOG(ERROR) << "Test with single div:";
  RunPerfTest(kIterationCount,
              *ToLayoutBox(
                  GetDocument().getElementById("singleDiv")->GetLayoutObject()),
              *view, rect);

  LOG(ERROR) << "Test with nested div:";
  RunPerfTest(kIterationCount,
              *ToLayoutBox(
                  GetDocument().getElementById("nestedDiv")->GetLayoutObject()),
              *view, rect);

  LOG(ERROR) << "Test with div nested under PaintLayers:";
  RunPerfTest(
      kIterationCount,
      *ToLayoutBox(
          GetDocument().getElementById("nestedPaintLayers")->GetLayoutObject()),
      *view, rect);

  LOG(ERROR) << "Test with div nested under transforms:";
  RunPerfTest(
      kIterationCount,
      *ToLayoutBox(
          GetDocument().getElementById("nestedTransform")->GetLayoutObject()),
      *view, rect);
}

}  // namespace blink
