// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "ui/gfx/geometry/mojom/geometry.mojom-blink.h"
#include "ui/gfx/geometry/mojom/geometry_traits_test_service.mojom-blink.h"

namespace blink {

namespace {

class GeometryStructTraitsTest
    : public testing::Test,
      public gfx::mojom::blink::GeometryTraitsTestService {
 public:
  GeometryStructTraitsTest() {}

 protected:
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService>
  GetTraitsTestProxy() {
    mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy;
    traits_test_receivers_.Add(this, proxy.BindNewPipeAndPassReceiver());
    return proxy;
  }

 private:
  // GeometryTraitsTestService:
  void EchoPoint(const WebPoint& p, EchoPointCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoPointF(const WebFloatPoint& p,
                  EchoPointFCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoPoint3F(const FloatPoint3D& p,
                   EchoPoint3FCallback callback) override {
    std::move(callback).Run(p);
  }

  void EchoSize(const WebSize& s, EchoSizeCallback callback) override {
    std::move(callback).Run(s);
  }

  void EchoSizeF(gfx::mojom::blink::SizeFPtr, EchoSizeFCallback) override {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoRect(const WebRect& r, EchoRectCallback callback) override {
    std::move(callback).Run(r);
  }

  void EchoRectF(const WebFloatRect& r, EchoRectFCallback callback) override {
    std::move(callback).Run(r);
  }

  void EchoInsets(gfx::mojom::blink::InsetsPtr, EchoInsetsCallback) override {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoInsetsF(gfx::mojom::blink::InsetsFPtr,
                   EchoInsetsFCallback) override {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoVector2d(gfx::mojom::blink::Vector2dPtr,
                    EchoVector2dCallback) override {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoVector2dF(gfx::mojom::blink::Vector2dFPtr,
                     EchoVector2dFCallback) override {
    // The type map is not specified.
    NOTREACHED();
  }

  void EchoVector3dF(const gfx::Vector3dF& v,
                     EchoVector3dFCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoQuaternion(const gfx::Quaternion& q,
                      EchoQuaternionCallback callback) override {
    std::move(callback).Run(q);
  }

  mojo::ReceiverSet<gfx::mojom::blink::GeometryTraitsTestService>
      traits_test_receivers_;

  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(GeometryStructTraitsTest);
};

}  // namespace

TEST_F(GeometryStructTraitsTest, Size) {
  const int32_t kWidth = 1234;
  const int32_t kHeight = 5678;
  WebSize input(kWidth, kHeight);
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy =
      GetTraitsTestProxy();
  WebSize output;
  proxy->EchoSize(input, &output);
  EXPECT_EQ(input, output);
}

TEST_F(GeometryStructTraitsTest, Point) {
  const float kX = 1234;
  const float kY = 5678;
  WebPoint input(kX, kY);
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy =
      GetTraitsTestProxy();
  WebPoint output;
  proxy->EchoPoint(input, &output);
  EXPECT_EQ(input, output);
}

TEST_F(GeometryStructTraitsTest, PointF) {
  const float kX = 1.234;
  const float kY = 5.678;
  WebFloatPoint input(kX, kY);
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy =
      GetTraitsTestProxy();
  WebFloatPoint output;
  proxy->EchoPointF(input, &output);
  EXPECT_EQ(input, output);
}

TEST_F(GeometryStructTraitsTest, Point3D) {
  const float kX = 1.234;
  const float kY = 5.678;
  const float kZ = 9.098;
  FloatPoint3D input(kX, kY, kZ);
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy =
      GetTraitsTestProxy();
  FloatPoint3D output;
  proxy->EchoPoint3F(input, &output);
  EXPECT_EQ(input, output);
}

TEST_F(GeometryStructTraitsTest, Rect) {
  const float kX = 1;
  const float kY = 2;
  const float kWidth = 3;
  const float kHeight = 4;
  WebRect input(kX, kY, kWidth, kHeight);
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy =
      GetTraitsTestProxy();
  WebRect output;
  proxy->EchoRect(input, &output);
  EXPECT_EQ(input, output);
}

TEST_F(GeometryStructTraitsTest, RectF) {
  const float kX = 1.234;
  const float kY = 2.345;
  const float kWidth = 3.456;
  const float kHeight = 4.567;
  WebFloatRect input(kX, kY, kWidth, kHeight);
  mojo::Remote<gfx::mojom::blink::GeometryTraitsTestService> proxy =
      GetTraitsTestProxy();
  WebFloatRect output;
  proxy->EchoRectF(input, &output);
  EXPECT_EQ(input, output);
}

}  // namespace blink
