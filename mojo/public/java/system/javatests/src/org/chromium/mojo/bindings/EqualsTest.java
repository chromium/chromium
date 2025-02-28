// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.mojo.HandleMock;
import org.chromium.mojo.bindings.test.mojom.mojo.StructOfObjects;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithFloat;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithFloat2;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithHandle;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithNestedArray;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithNullables;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithObjectArrays;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithPrimitiveArrays;
import org.chromium.mojo.bindings.test.mojom.mojo.StructWithPrimitives;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionOfObjects;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionWithFloat;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionWithFloat2;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionWithHandle;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionWithObjectArrays;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionWithPrimitiveArrays;
import org.chromium.mojo.bindings.test.mojom.mojo.UnionWithPrimitives;

/**
 * Tests for the equals logic of the generated structs/unions, using structs/unions defined in
 * mojo/public/interfaces/bindings/tests/equals_test_structs.test-mojom .
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@NullMarked
@SuppressWarnings("ArgumentSelectionDefectChecker")
public class EqualsTest {
    @Test
    @SmallTest
    public void testStructWithPrimitives() {
        StructWithPrimitives first = new StructWithPrimitives();
        first.a = 5;
        first.b = true;
        StructWithPrimitives second = new StructWithPrimitives();
        second.a = 5;
        second.b = true;
        StructWithPrimitives third = new StructWithPrimitives();
        third.a = 7;
        third.b = false;
        StructWithPrimitives fourth = new StructWithPrimitives();
        fourth.a = 5;
        fourth.b = false;

        checkEqualWithHashCode(first, second);
        checkEqualWithHashCode(second, first);
        checkNotEqual(first, third);
        checkNotEqual(second, fourth);
        checkNotEqual(first, null);
        checkNotEqual(first, new Object());
    }

    @Test
    @SmallTest
    public void testStructWithPrimitiveArrays() {
        StructWithPrimitiveArrays first = new StructWithPrimitiveArrays();
        first.a = new int[] {1, 2, 3, 4};
        first.b = new float[] {4.0f, 3.0f, 2.0f, 1.0f};
        StructWithPrimitiveArrays second = new StructWithPrimitiveArrays();
        second.a = first.a;
        second.b = first.b;
        StructWithPrimitiveArrays third = new StructWithPrimitiveArrays();
        third.a = new int[] {1, 2, 3, 4};
        third.b = new float[] {4.0f, 3.0f, 2.0f, 1.0f};
        StructWithPrimitiveArrays fourth = new StructWithPrimitiveArrays();
        fourth.a = new int[] {1, 4, 3, 2};
        fourth.b = new float[] {4.0f, 2.0f, 2.0f, 1.0f};

        checkEqualWithHashCode(first, second);
        checkEqualWithHashCode(second, first);
        checkEqualWithHashCode(first, third);
        checkNotEqual(second, fourth);
    }

    @Test
    @SmallTest
    public void testStructOfObjects() {
        StructWithPrimitives sub = new StructWithPrimitives();
        sub.a = 5;
        sub.b = true;
        StructWithPrimitives sub2 = new StructWithPrimitives();
        sub2.a = 5;
        sub2.b = true;
        StructWithPrimitives sub3 = new StructWithPrimitives();
        sub3.a = 7;
        sub3.b = false;
        StructOfObjects first = new StructOfObjects();
        first.a = sub;
        StructOfObjects second = new StructOfObjects();
        second.a = sub2;
        StructOfObjects third = new StructOfObjects();
        third.a = sub3;

        checkEqualWithHashCode(first, second);
        checkEqualWithHashCode(second, first);
        checkNotEqual(first, third);
    }

    @Test
    @SmallTest
    public void testStructWithObjectArrays() {
        StructWithPrimitives[] arr = new StructWithPrimitives[] {new StructWithPrimitives()};
        arr[0].a = 7;
        StructWithObjectArrays first = new StructWithObjectArrays();
        first.a = arr;
        StructWithPrimitives[] arr2 = new StructWithPrimitives[] {new StructWithPrimitives()};
        arr2[0].a = 5;
        StructWithObjectArrays second = new StructWithObjectArrays();
        second.a = arr2;
        StructWithObjectArrays third = new StructWithObjectArrays();
        third.a = arr;
        third.b = new Float[] {5.0f};
        StructWithObjectArrays fourth = new StructWithObjectArrays();
        fourth.a = arr;
        fourth.b = new Float[] {null};

        checkNotEqual(first, second);
        checkNotEqual(third, fourth);
    }

    @Test
    @SmallTest
    public void testStructWithHandle() {
        StructWithHandle first = new StructWithHandle();
        first.a = new HandleMock();
        StructWithHandle second = new StructWithHandle();
        second.a = first.a;
        StructWithHandle third = new StructWithHandle();
        third.a = new HandleMock();

        checkEqualWithHashCode(first, second);
        checkNotEqual(first, third);
    }

    @Test
    @SmallTest
    public void testStructWithNullables() {
        StructWithNullables first = new StructWithNullables();
        first.a = new StructWithPrimitives();
        first.a.a = 5;
        first.a.b = true;
        first.b = "hello world";
        first.c = false;
        StructWithNullables second = new StructWithNullables();
        second.a = new StructWithPrimitives();
        second.a.a = 4;
        second.a.b = true;
        second.b = new String(first.b);
        second.c = first.c;
        StructWithNullables third = new StructWithNullables();
        third.a = first.a;
        third.b = null;
        third.c = first.c;
        StructWithNullables fourth = new StructWithNullables();
        fourth.a = first.a;
        fourth.b = first.b;
        fourth.c = null;

        checkNotEqual(first, second);
        checkNotEqual(first, third);
        checkNotEqual(first, fourth);
    }

    @Test
    @SmallTest
    public void testNaNBehavior() {
        StructWithFloat first = new StructWithFloat();
        first.a = Float.NaN;
        StructWithFloat second = new StructWithFloat();
        second.a = Float.NaN;

        // Check reflexivity
        checkEqualWithHashCode(first, first);
        checkEqualWithHashCode(first, second);
    }

    @Test
    @SmallTest
    public void testFloatZeroBehavior() {
        StructWithFloat first = new StructWithFloat();
        first.a = -0.0f;
        StructWithFloat second = new StructWithFloat();
        second.a = 0.0f;

        // Check reflexivity
        checkEqualWithHashCode(first, first);
        checkNotEqual(first, second);
    }

    @Test
    @SmallTest
    public void testDifferentStructTypesAreNotEqual() {
        StructWithFloat first = new StructWithFloat();
        first.a = 5.0f;
        StructWithFloat2 second = new StructWithFloat2();
        second.a = 5.0f;

        checkNotEqual(first, second);
    }

    @Test
    @SmallTest
    public void testNestedArrays() {
        StructWithNestedArray first = new StructWithNestedArray();
        first.a = new int[][] {{0, 1}, {2, 3}};
        StructWithNestedArray second = new StructWithNestedArray();
        second.a = new int[][] {{0, 1}, {2, 3}};
        StructWithNestedArray third = new StructWithNestedArray();
        third.a = new int[][] {{0, 1}, {2, 4}};

        checkEqualWithHashCode(first, second);
        checkNotEqual(first, third);
    }

    @Test
    @SmallTest
    public void testUnionWithPrimitives() {
        UnionWithPrimitives first = new UnionWithPrimitives();
        first.setA((byte) 5);
        first.setB(true);
        UnionWithPrimitives second = new UnionWithPrimitives();
        second.setA((byte) 5);
        second.setB(true);
        UnionWithPrimitives third = new UnionWithPrimitives();
        third.setA((byte) 7);
        third.setB(false);
        // Check that unions with different tags aren't equal.
        UnionWithPrimitives fourth = new UnionWithPrimitives();
        fourth.setB(true);
        fourth.setA((byte) 5);

        checkEqualWithHashCode(first, second);
        checkEqualWithHashCode(second, first);
        checkNotEqual(first, third);
        checkNotEqual(first, fourth);
        checkNotEqual(first, null);
        checkNotEqual(first, new Object());
    }

    @Test
    @SmallTest
    public void testUnionWithPrimitiveArrays() {
        UnionWithPrimitiveArrays first = new UnionWithPrimitiveArrays();
        first.setA(new int[] {1, 2, 3, 4});
        UnionWithPrimitiveArrays second = new UnionWithPrimitiveArrays();
        second.setA(first.getA());
        UnionWithPrimitiveArrays third = new UnionWithPrimitiveArrays();
        third.setA(new int[] {1, 2, 3, 4});
        UnionWithPrimitiveArrays fourth = new UnionWithPrimitiveArrays();
        fourth.setA(new int[] {1, 2, 4, 5});

        checkEqualWithHashCode(first, second);
        checkEqualWithHashCode(third, first);
        checkNotEqual(first, fourth);
    }

    @Test
    @SmallTest
    public void testUnionOfObjects() {
        StructWithPrimitives sub = new StructWithPrimitives();
        sub.a = 5;
        sub.b = true;
        StructWithPrimitives sub2 = new StructWithPrimitives();
        sub2.a = 5;
        sub2.b = true;
        StructWithPrimitives sub3 = new StructWithPrimitives();
        sub3.a = 7;
        sub3.b = false;
        UnionOfObjects first = new UnionOfObjects();
        first.setA(sub);
        UnionOfObjects second = new UnionOfObjects();
        second.setA(sub2);
        UnionOfObjects third = new UnionOfObjects();
        third.setA(sub3);

        checkEqualWithHashCode(first, second);
        checkEqualWithHashCode(second, first);
        checkNotEqual(first, third);
    }

    @Test
    @SmallTest
    public void testUnionWithObjectArrays() {
        UnionWithPrimitives[] arr = new UnionWithPrimitives[] {new UnionWithPrimitives()};
        arr[0].setA((byte) 7);
        UnionWithObjectArrays first = new UnionWithObjectArrays();
        first.setA(arr);

        UnionWithPrimitives[] arr2 = new UnionWithPrimitives[] {new UnionWithPrimitives()};
        arr2[0].setA((byte) 7);
        UnionWithObjectArrays second = new UnionWithObjectArrays();
        second.setA(arr2);

        UnionWithPrimitives[] arr3 = new UnionWithPrimitives[] {new UnionWithPrimitives()};
        arr3[0].setA((byte) 5);
        UnionWithObjectArrays third = new UnionWithObjectArrays();
        third.setA(arr3);

        checkEqualWithHashCode(first, second);
        checkNotEqual(first, third);
    }

    @Test
    @SmallTest
    public void testUnionWithHandle() {
        UnionWithHandle first = new UnionWithHandle();
        first.setA(new HandleMock());
        UnionWithHandle second = new UnionWithHandle();
        second.setA(first.getA());
        UnionWithHandle third = new UnionWithHandle();
        third.setA(new HandleMock());

        checkEqualWithHashCode(first, second);
        checkNotEqual(first, third);
    }

    @Test
    @SmallTest
    public void testNaNBehaviorForUnions() {
        UnionWithFloat first = new UnionWithFloat();
        first.setA(Float.NaN);
        UnionWithFloat second = new UnionWithFloat();
        second.setA(Float.NaN);

        // Check reflexivity
        checkEqualWithHashCode(first, first);
        checkEqualWithHashCode(first, second);
    }

    @Test
    @SmallTest
    public void testFloatZeroBehaviorForUnions() {
        UnionWithFloat first = new UnionWithFloat();
        first.setA(-0.0f);
        UnionWithFloat second = new UnionWithFloat();
        second.setA(0.0f);

        // Check reflexivity
        checkEqualWithHashCode(first, first);
        checkNotEqual(first, second);
    }

    @Test
    @SmallTest
    public void testDifferentUnionTypesAreNotEqual() {
        UnionWithFloat first = new UnionWithFloat();
        first.setA(5.0f);
        UnionWithFloat2 second = new UnionWithFloat2();
        second.setA(5.0f);

        checkNotEqual(first, second);
    }

    @Test
    @SmallTest
    public void testActiveVariantIsCompared() {
        UnionWithPrimitives first = new UnionWithPrimitives();
        first.setA((byte) 5);
        first.setB(true);
        UnionWithPrimitives second = new UnionWithPrimitives();
        second.setA((byte) 999999);
        second.setB(true);

        checkEqualWithHashCode(second, first);
    }

    private void checkEqualWithHashCode(Object first, @Nullable Object second) {
        Assert.assertTrue(first.equals(second));
        Assert.assertEquals(first.hashCode(), second.hashCode());
    }

    private void checkNotEqual(Object first, @Nullable Object second) {
        Assert.assertFalse(first.equals(second));
    }
}
