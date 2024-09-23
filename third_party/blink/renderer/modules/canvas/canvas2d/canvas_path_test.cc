// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_path.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/geometry/skia_conversions.h"

// GoogleTest macros trigger a bug in IWYU:
// https://github.com/include-what-you-use/include-what-you-use/issues/1546
// IWYU pragma: no_include <string>

namespace blink {

class CanvasPathTest : public testing::Test {
 public:
  CanvasPathTest() = default;
  ~CanvasPathTest() override { context_->NotifyContextDestroyed(); }

 protected:
  test::TaskEnvironment task_environment_;
  Persistent<ExecutionContext> context_ =
      MakeGarbageCollected<NullExecutionContext>();
};

class TestCanvasPath : public GarbageCollected<TestCanvasPath>,
                       public CanvasPath {
 public:
  explicit TestCanvasPath(ExecutionContext* context)
      : execution_context_(context) {}

  ExecutionContext* GetTopExecutionContext() const override {
    return execution_context_.Get();
  }

  void Trace(Visitor* v) const override {
    CanvasPath::Trace(v);
    v->Trace(execution_context_);
  }

 private:
  Member<ExecutionContext> execution_context_;
};

TEST_F(CanvasPathTest, Line) {
  CanvasPath* path = MakeGarbageCollected<TestCanvasPath>(context_);
  EXPECT_FALSE(path->IsLine());
  EXPECT_TRUE(path->IsEmpty());
  const gfx::PointF start(0, 1);
  path->moveTo(start.x(), start.y());
  EXPECT_FALSE(path->IsEmpty());
  EXPECT_FALSE(path->IsLine());
  const gfx::PointF end(2, 3);
  path->lineTo(end.x(), end.y());
  EXPECT_TRUE(path->IsLine());
  EXPECT_EQ(path->line().start, start);
  EXPECT_EQ(path->line().end, end);
}

TEST_F(CanvasPathTest, LineBoundingRect) {
  CanvasPath* path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF start(0, 1);
  path->moveTo(start.x(), start.y());
  const gfx::PointF end(2, 3);
  path->lineTo(end.x(), end.y());
  EXPECT_TRUE(path->IsLine());

  SkPath sk_path;
  sk_path.moveTo(gfx::PointFToSkPoint(start));
  sk_path.lineTo(gfx::PointFToSkPoint(end));
  Path path_from_sk_path(sk_path);

  EXPECT_EQ(path->BoundingRect(), path_from_sk_path.BoundingRect());
}

TEST_F(CanvasPathTest, LineEquality) {
  CanvasPath* path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF start(0, 1);
  path->moveTo(start.x(), start.y());
  const gfx::PointF end(2, 3);
  path->lineTo(end.x(), end.y());
  EXPECT_TRUE(path->IsLine());

  Path path2;
  path2.MoveTo(start);
  path2.AddLineTo(end);

  EXPECT_EQ(path->GetPath(), path2);
}

TEST_F(CanvasPathTest, LineEquality2) {
  CanvasPath* path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF start(0, 1);
  path->moveTo(start.x(), start.y());
  Path path2;
  path2.MoveTo(start);
  EXPECT_EQ(path->GetPath(), path2);

  const gfx::PointF end(2, 3);
  path->lineTo(end.x(), end.y());
  EXPECT_TRUE(path->IsLine());

  path2.AddLineTo(end);

  EXPECT_EQ(path->GetPath(), path2);
}

TEST_F(CanvasPathTest, MultipleMoveTos) {
  CanvasPath* path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF start(0, 1);
  path->moveTo(start.x(), start.y());
  const gfx::PointF next(2, 3);
  path->moveTo(next.x(), next.y());

  SkPath sk_path;
  sk_path.moveTo(gfx::PointFToSkPoint(start));
  sk_path.moveTo(gfx::PointFToSkPoint(next));
  Path path_from_sk_path(sk_path);

  EXPECT_EQ(path->GetPath(), path_from_sk_path);
}

TEST_F(CanvasPathTest, RectMoveToLineTo) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::RectF rect(1, 2, 3, 4);
  const gfx::PointF start(0, 1);
  const gfx::PointF end(2, 3);
  canvas_path->rect(rect.x(), rect.y(), rect.width(), rect.height());
  canvas_path->moveTo(start.x(), start.y());
  canvas_path->lineTo(end.x(), end.y());
  EXPECT_FALSE(canvas_path->IsEmpty());
  EXPECT_FALSE(canvas_path->IsLine());
  Path path;
  path.AddRect(rect);
  path.MoveTo(start);
  path.AddLineTo(end);
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, MoveToLineToRect) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::RectF rect(1, 2, 3, 4);
  const gfx::PointF start(0, 1);
  const gfx::PointF end(2, 3);
  canvas_path->moveTo(start.x(), start.y());
  canvas_path->lineTo(end.x(), end.y());
  canvas_path->rect(rect.x(), rect.y(), rect.width(), rect.height());
  EXPECT_FALSE(canvas_path->IsEmpty());
  EXPECT_FALSE(canvas_path->IsLine());
  Path path;
  path.MoveTo(start);
  path.AddLineTo(end);
  path.AddRect(rect);
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, OnlyLineTo) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF end(2, 3);
  canvas_path->lineTo(end.x(), end.y());
  EXPECT_FALSE(canvas_path->IsEmpty());
  EXPECT_TRUE(canvas_path->IsLine());
  // CanvasPath::lineTo() when empty implicitly does a moveto.
  Path path;
  path.MoveTo(end);
  path.AddLineTo(end);
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, LineToLineTo) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF start(1, -1);
  const gfx::PointF end(2, 3);
  canvas_path->lineTo(start.x(), start.y());
  canvas_path->lineTo(end.x(), end.y());
  EXPECT_FALSE(canvas_path->IsEmpty());
  EXPECT_FALSE(canvas_path->IsLine());
  // CanvasPath::lineTo() when empty implicitly does a moveto.
  Path path;
  path.MoveTo(start);
  path.AddLineTo(start);
  path.AddLineTo(end);
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, MoveToLineToMoveTo) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF p1(1, -1);
  const gfx::PointF p2(2, 3);
  const gfx::PointF p3(2, 3);
  canvas_path->moveTo(p1.x(), p1.y());
  canvas_path->lineTo(p2.x(), p2.y());
  canvas_path->moveTo(p3.x(), p3.y());
  EXPECT_FALSE(canvas_path->IsEmpty());
  EXPECT_FALSE(canvas_path->IsLine());
  Path path;
  path.MoveTo(p1);
  path.AddLineTo(p2);
  path.MoveTo(p3);
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, MoveToMoveToLineTo) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF p1(1, -1);
  const gfx::PointF p2(2, 3);
  const gfx::PointF p3(2, 3);
  canvas_path->moveTo(p1.x(), p1.y());
  canvas_path->moveTo(p2.x(), p2.y());
  canvas_path->lineTo(p3.x(), p3.y());
  EXPECT_FALSE(canvas_path->IsEmpty());
  EXPECT_FALSE(canvas_path->IsLine());
  Path path;
  path.MoveTo(p1);
  path.MoveTo(p2);
  path.AddLineTo(p3);
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, MoveToLineClosePath) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  const gfx::PointF p1(1, -1);
  const gfx::PointF p2(2, 3);
  canvas_path->moveTo(p1.x(), p1.y());
  canvas_path->lineTo(p2.x(), p2.y());
  canvas_path->closePath();
  EXPECT_FALSE(canvas_path->IsEmpty());
  // closePath() cancels the line.
  EXPECT_FALSE(canvas_path->IsLine());

  Path path;
  path.MoveTo(p1);
  path.AddLineTo(p2);
  path.CloseSubpath();
  EXPECT_EQ(canvas_path->GetPath(), path);
}

TEST_F(CanvasPathTest, Arc) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  NonThrowableExceptionState exception_state;
  canvas_path->arc(0, 1, 5, 2, 3, false, exception_state);
  EXPECT_TRUE(canvas_path->IsArc());

  Path path;
  path.AddArc(gfx::PointF(0, 1), 5, 2, 3);
  EXPECT_EQ(canvas_path->GetPath(), path);
  EXPECT_TRUE(canvas_path->IsArc());

  canvas_path->closePath();
  path.CloseSubpath();
  EXPECT_EQ(canvas_path->GetPath(), path);
  EXPECT_TRUE(canvas_path->IsArc());
}

TEST_F(CanvasPathTest, ArcThenLine) {
  CanvasPath* canvas_path = MakeGarbageCollected<TestCanvasPath>(context_);
  NonThrowableExceptionState exception_state;
  canvas_path->arc(0, 1, 5, 2, 3, false, exception_state);
  EXPECT_TRUE(canvas_path->IsArc());
  canvas_path->lineTo(8, 9);
  EXPECT_FALSE(canvas_path->IsArc());
  EXPECT_FALSE(canvas_path->IsLine());

  Path path;
  path.AddArc(gfx::PointF(0, 1), 5, 2, 3);
  path.AddLineTo(gfx::PointF(8, 9));
  EXPECT_EQ(canvas_path->GetPath(), path);
}

}  // namespace blink
