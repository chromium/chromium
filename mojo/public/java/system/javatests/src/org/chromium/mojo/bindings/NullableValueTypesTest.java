// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.mojo.bindings;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.mojo.MojoTestRule;
import org.chromium.mojo.bindings.test.mojom.nullable_value_types.ExtensibleEnum;
import org.chromium.mojo.bindings.test.mojom.nullable_value_types.InterfaceV2;
import org.chromium.mojo.bindings.test.mojom.nullable_value_types.RegularEnum;
import org.chromium.mojo.bindings.test.mojom.nullable_value_types.StructWithEnums;
import org.chromium.mojo.bindings.test.mojom.nullable_value_types.StructWithNumerics;
import org.chromium.mojo.bindings.test.mojom.nullable_value_types.TypemappedEnum;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.util.HashMap;
import java.util.Map;

/**
 * Testing the Java bindings implementation for nullable value types, e.g. `int32?`. Note that the
 * Java tests are not exhaustive; it is assumed that successful interop with a C++ implementation
 * over JNI transitively implies correctness from the exhaustive C++ tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class NullableValueTypesTest {
    @Rule public MojoTestRule mTestRule = new MojoTestRule();

    private InterfaceV2 getRemote() {
        Pair<InterfaceV2.Proxy, InterfaceRequest<InterfaceV2>> result =
                InterfaceV2.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
        NullableValueTypesTestUtilJni.get()
                .bindTestInterface(result.second.passHandle().releaseNativeHandle());
        return result.first;
    }

    /** Test cases when nullable value type fields are null. */
    @Test
    @SmallTest
    public void testNull() {
        final var remote = getRemote();

        {
            final @RegularEnum.EnumType Integer inEnumValue = null;
            final @TypemappedEnum.EnumType Integer inMappedEnumValue = null;

            remote.methodWithEnums(
                    inEnumValue,
                    inMappedEnumValue,
                    (@Nullable @RegularEnum.EnumType Integer outEnumValue,
                            @Nullable @TypemappedEnum.EnumType Integer outMappedEnumValue) -> {
                        Assert.assertNull(outEnumValue);
                        Assert.assertNull(outMappedEnumValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            final var in = new StructWithEnums();
            Assert.assertNull(in.enumValue);
            Assert.assertNull(in.mappedEnumValue);

            remote.methodWithStructWithEnums(
                    in,
                    (StructWithEnums out) -> {
                        Assert.assertNull(out.enumValue);
                        Assert.assertNull(out.mappedEnumValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            final Boolean inBoolValue = null;
            final Byte inU8Value = null;
            final Short inU16Value = null;
            final Integer inU32Value = null;
            final Long inU64Value = null;
            final Byte inI8Value = null;
            final Short inI16Value = null;
            final Integer inI32Value = null;
            final Long inI64Value = null;
            final Float inFloatValue = null;
            final Double inDoubleValue = null;

            remote.methodWithNumerics(
                    inBoolValue,
                    inU8Value,
                    inU16Value,
                    inU32Value,
                    inU64Value,
                    inI8Value,
                    inI16Value,
                    inI32Value,
                    inI64Value,
                    inFloatValue,
                    inDoubleValue,
                    (@Nullable Boolean outBoolValue,
                            @Nullable Byte outU8Value,
                            @Nullable Short outU16Value,
                            @Nullable Integer outU32Value,
                            @Nullable Long outU64Value,
                            @Nullable Byte outI8Value,
                            @Nullable Short outI16Value,
                            @Nullable Integer outI32Value,
                            @Nullable Long outI64Value,
                            @Nullable Float outFloatValue,
                            @Nullable Double outDoubleValue) -> {
                        Assert.assertNull(outBoolValue);
                        Assert.assertNull(outU8Value);
                        Assert.assertNull(outU16Value);
                        Assert.assertNull(outU32Value);
                        Assert.assertNull(outU64Value);
                        Assert.assertNull(outI8Value);
                        Assert.assertNull(outI16Value);
                        Assert.assertNull(outI32Value);
                        Assert.assertNull(outI64Value);
                        Assert.assertNull(outFloatValue);
                        Assert.assertNull(outDoubleValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            final var in = new StructWithNumerics();
            Assert.assertNull(in.boolValue);
            Assert.assertNull(in.u8Value);
            Assert.assertNull(in.u16Value);
            Assert.assertNull(in.u32Value);
            Assert.assertNull(in.u64Value);
            Assert.assertNull(in.i8Value);
            Assert.assertNull(in.i16Value);
            Assert.assertNull(in.i32Value);
            Assert.assertNull(in.i64Value);
            Assert.assertNull(in.floatValue);
            Assert.assertNull(in.doubleValue);

            remote.methodWithStructWithNumerics(
                    in,
                    (StructWithNumerics out) -> {
                        Assert.assertNull(out.boolValue);
                        Assert.assertNull(out.u8Value);
                        Assert.assertNull(out.u16Value);
                        Assert.assertNull(out.u32Value);
                        Assert.assertNull(out.u64Value);
                        Assert.assertNull(out.i8Value);
                        Assert.assertNull(out.i16Value);
                        Assert.assertNull(out.i32Value);
                        Assert.assertNull(out.i64Value);
                        Assert.assertNull(out.floatValue);
                        Assert.assertNull(out.doubleValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        // Versioned interfaces intentionally untested... for now.
    }

    /** Test cases when nullable value type fields are set. */
    @Test
    @SmallTest
    public void testSet() {
        final var remote = getRemote();

        {
            final @RegularEnum.EnumType Integer inEnumValue = RegularEnum.THIS_VALUE;
            final @TypemappedEnum.EnumType Integer inMappedEnumValue =
                    TypemappedEnum.THIS_OTHER_VALUE;

            remote.methodWithEnums(
                    inEnumValue,
                    inMappedEnumValue,
                    (@Nullable @RegularEnum.EnumType Integer outEnumValue,
                            @Nullable @TypemappedEnum.EnumType Integer outMappedEnumValue) -> {
                        Assert.assertEquals(inEnumValue, outEnumValue);
                        Assert.assertEquals(inMappedEnumValue, outMappedEnumValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            final var in = new StructWithEnums();
            in.enumValue = RegularEnum.THAT_VALUE;
            in.mappedEnumValue = TypemappedEnum.THAT_OTHER_VALUE;

            remote.methodWithStructWithEnums(
                    in,
                    (StructWithEnums out) -> {
                        Assert.assertEquals(in.enumValue, out.enumValue);
                        Assert.assertEquals(in.mappedEnumValue, out.mappedEnumValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            final Boolean inBoolValue = false;
            final Byte inU8Value = 64;
            final Short inU16Value = 256;
            final Integer inU32Value = 1024;
            final Long inU64Value = 4096L;
            final Byte inI8Value = -64;
            final Short inI16Value = -256;
            final Integer inI32Value = -1024;
            final Long inI64Value = -4096L;
            final Float inFloatValue = 2.25f;
            final Double inDoubleValue = -9.0;

            remote.methodWithNumerics(
                    inBoolValue,
                    inU8Value,
                    inU16Value,
                    inU32Value,
                    inU64Value,
                    inI8Value,
                    inI16Value,
                    inI32Value,
                    inI64Value,
                    inFloatValue,
                    inDoubleValue,
                    (@Nullable Boolean outBoolValue,
                            @Nullable Byte outU8Value,
                            @Nullable Short outU16Value,
                            @Nullable Integer outU32Value,
                            @Nullable Long outU64Value,
                            @Nullable Byte outI8Value,
                            @Nullable Short outI16Value,
                            @Nullable Integer outI32Value,
                            @Nullable Long outI64Value,
                            @Nullable Float outFloatValue,
                            @Nullable Double outDoubleValue) -> {
                        Assert.assertEquals(inBoolValue, outBoolValue);
                        Assert.assertEquals(inU8Value, outU8Value);
                        Assert.assertEquals(inU16Value, outU16Value);
                        Assert.assertEquals(inU32Value, outU32Value);
                        Assert.assertEquals(inU64Value, outU64Value);
                        Assert.assertEquals(inI8Value, outI8Value);
                        Assert.assertEquals(inI16Value, outI16Value);
                        Assert.assertEquals(inI32Value, outI32Value);
                        Assert.assertEquals(inI64Value, outI64Value);
                        Assert.assertEquals(inFloatValue, outFloatValue);
                        Assert.assertEquals(inDoubleValue, outDoubleValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            final var in = new StructWithNumerics();
            in.boolValue = true;
            in.u8Value = 8;
            in.u16Value = 16;
            in.u32Value = 32;
            in.u64Value = 64L;
            in.i8Value = -8;
            in.i16Value = -16;
            in.i32Value = -32;
            in.i64Value = -64L;
            in.floatValue = -1.5f;
            in.doubleValue = 3.0;

            remote.methodWithStructWithNumerics(
                    in,
                    (StructWithNumerics out) -> {
                        Assert.assertEquals(in.boolValue, out.boolValue);
                        Assert.assertEquals(in.u8Value, out.u8Value);
                        Assert.assertEquals(in.u16Value, out.u16Value);
                        Assert.assertEquals(in.u32Value, out.u32Value);
                        Assert.assertEquals(in.u64Value, out.u64Value);
                        Assert.assertEquals(in.i8Value, out.i8Value);
                        Assert.assertEquals(in.i16Value, out.i16Value);
                        Assert.assertEquals(in.i32Value, out.i32Value);
                        Assert.assertEquals(in.i64Value, out.i64Value);
                        Assert.assertEquals(in.floatValue, out.floatValue);
                        Assert.assertEquals(in.doubleValue, out.doubleValue);
                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            // No specific pattern to the tests. We just want a mixture of values and nulls.
            Boolean[] boolValues = {true, true, true, null, false, false, false};
            Byte[] uByteValues = {null, (byte) 8, null};
            Short[] uShortValues = {null, (short) 1, (short) 6, null};
            Integer[] uIntValues = {null, 3, 2, null};
            Long[] uLongValues = {null, 6L, 4L, null};
            Byte[] byteValues = {null, (byte) 3, (byte) 2, null};
            Short[] shortValues = {null, (short) 3, (short) 2, null};
            Integer[] intValues = {null, 3, 2, null};
            Long[] longValues = {null, 3L, 2L, null};
            Float[] floatValues = {null, 4.0f, 2.0f, null};
            Double[] doubleValues = {null, 6.0, 4.0, null};
            @RegularEnum.EnumType Integer[] enumValues = {RegularEnum.THIS_VALUE, null};
            // Explicitly test defaulting behaviour.
            @ExtensibleEnum.EnumType
            Integer[] extensibleEnumValues = {null, 555, ExtensibleEnum.UNKNOWN};
            Map<Integer, Boolean> boolMap = new HashMap();
            boolMap.put(0, true);
            boolMap.put(1, null);
            boolMap.put(2, false);
            Map<Integer, Integer> intMap = new HashMap();
            intMap.put(0, 10);
            intMap.put(1, null);
            intMap.put(2, 12);

            remote.methodWithContainers(
                    boolValues,
                    uByteValues,
                    uShortValues,
                    uIntValues,
                    uLongValues,
                    byteValues,
                    shortValues,
                    intValues,
                    longValues,
                    floatValues,
                    doubleValues,
                    enumValues,
                    extensibleEnumValues,
                    boolMap,
                    intMap,
                    (Boolean[] outBoolValues,
                            Byte[] outUByteValues,
                            Short[] outUShortValues,
                            Integer[] outUIntValues,
                            Long[] outULongValues,
                            Byte[] outByteValues,
                            Short[] outShortValues,
                            Integer[] outIntValues,
                            Long[] outLongValues,
                            Float[] outFloatValues,
                            Double[] outDoubleValues,
                            @RegularEnum.EnumType Integer[] outEnumValues,
                            @ExtensibleEnum.EnumType Integer[] outExtensibleEnumValues,
                            Map<Integer, Boolean> outBoolMap,
                            Map<Integer, Integer> outIntMap) -> {
                        Assert.assertArrayEquals(boolValues, outBoolValues);
                        Assert.assertArrayEquals(uByteValues, outUByteValues);
                        Assert.assertArrayEquals(uShortValues, outUShortValues);
                        Assert.assertArrayEquals(uIntValues, outUIntValues);
                        Assert.assertArrayEquals(uLongValues, outULongValues);
                        Assert.assertArrayEquals(byteValues, outByteValues);
                        Assert.assertArrayEquals(shortValues, outShortValues);
                        Assert.assertArrayEquals(intValues, outIntValues);
                        Assert.assertArrayEquals(longValues, outLongValues);
                        Assert.assertArrayEquals(floatValues, outFloatValues);
                        Assert.assertArrayEquals(doubleValues, outDoubleValues);
                        Assert.assertArrayEquals(enumValues, outEnumValues);
                        Assert.assertArrayEquals(
                                new Integer[] {
                                    null, ExtensibleEnum.UNKNOWN, ExtensibleEnum.UNKNOWN
                                },
                                outExtensibleEnumValues);
                        Assert.assertEquals(boolMap, outBoolMap);
                        Assert.assertEquals(intMap, outIntMap);

                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            // Test bitfields that extend beyond the size of the element.
            final int testSize = 1000;
            Integer[] intValues = new Integer[testSize];
            for (int i = 0; i < intValues.length; ++i) {
                // Alternate between value and null.
                intValues[i] = i % 2 == 0 ? i : null;
            }

            // Also test the case where we pass in empty containers.
            remote.methodWithContainers(
                    new Boolean[0],
                    new Byte[0],
                    new Short[0],
                    intValues,
                    new Long[0],
                    new Byte[0],
                    new Short[0],
                    new Integer[0],
                    new Long[0],
                    new Float[0],
                    new Double[0],
                    new Integer[0],
                    new Integer[0],
                    new HashMap<Integer, Boolean>(),
                    new HashMap<Integer, Integer>(),
                    (Boolean[] outBoolValues,
                            Byte[] outUByteValues,
                            Short[] outUShortValues,
                            Integer[] outUIntValues,
                            Long[] outULongValues,
                            Byte[] outByteValues,
                            Short[] outShortValues,
                            Integer[] outIntValues,
                            Long[] outLongValues,
                            Float[] outFloatValues,
                            Double[] outDoubleValues,
                            @RegularEnum.EnumType Integer[] outEnumValues,
                            @ExtensibleEnum.EnumType Integer[] outExtensibleEnumValues,
                            Map<Integer, Boolean> outBoolMap,
                            Map<Integer, Integer> outIntMap) -> {
                        Assert.assertArrayEquals(intValues, outUIntValues);

                        mTestRule.quitLoop();
                    });
            mTestRule.runLoopForever();
        }

        {
            remote.methodToSendUnknownEnum(
                    (@ExtensibleEnum.EnumType Integer[] result) -> {
                        Assert.assertArrayEquals(
                                new Integer[] {ExtensibleEnum.UNKNOWN, null}, result);

                        mTestRule.quitLoop();
                    });

            mTestRule.runLoopForever();
        }

        // Versioned interfaces intentionally untested... for now.
    }

    // TODO(dcheng): Add a test with C++ remote and Java receiver.
}
