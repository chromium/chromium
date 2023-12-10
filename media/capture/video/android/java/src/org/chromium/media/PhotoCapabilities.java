// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/**
 * Set of PhotoCapabilities read from the different VideoCapture Devices.
 **/
@JNINamespace("media")
class PhotoCapabilities {
    public boolean mBoolCapability[]; // boolean values, indexed by PhotoCapabilityBool
    public double mDoubleCapability[]; // double values, indexed by PhotoCapabilityDouble
    public int mIntCapability[]; // int values, indexed by PhotoCapabilityInt
    public int mFillLightModeArray[]; // list of AndroidFillLightMode values
    public int mMeteringMode[]; // AndroidMeteringMode values, indexed
    // by MeteringModeType
    public int mMeteringModeArray[][]; // lists of AndroidMeteringMode values,

    // indexed by MeteringModeType

    PhotoCapabilities(
            boolean[] boolCapability,
            double[] doubleCapability,
            int[] intCapability,
            int[] fillLightModeArray,
            int[] meteringMode,
            int[][] meteringModeArray) {
        if (boolCapability.length != PhotoCapabilityBool.NUM_ENTRIES
                || doubleCapability.length != PhotoCapabilityDouble.NUM_ENTRIES
                || intCapability.length != PhotoCapabilityInt.NUM_ENTRIES
                || meteringMode.length != MeteringModeType.NUM_ENTRIES
                || meteringModeArray.length != MeteringModeType.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        if (fillLightModeArray != null) {
            for (int i = 0; i < fillLightModeArray.length; i++) {
                if (fillLightModeArray[i] < 0
                        || fillLightModeArray[i] >= AndroidFillLightMode.NUM_ENTRIES) {
                    throw new IllegalArgumentException();
                }
            }
        }
        for (int i = 0; i < meteringMode.length; i++) {
            if (meteringMode[i] < 0 || meteringMode[i] >= AndroidMeteringMode.NUM_ENTRIES) {
                throw new IllegalArgumentException();
            }
        }
        for (int i = 0; i < meteringModeArray.length; i++) {
            if (meteringModeArray[i] == null) continue;
            for (int j = 0; j < meteringModeArray[i].length; j++) {
                if (meteringModeArray[i][j] < 0
                        || meteringModeArray[i][j] >= AndroidMeteringMode.NUM_ENTRIES) {
                    throw new IllegalArgumentException();
                }
            }
        }

        mBoolCapability = boolCapability.clone();
        mDoubleCapability = doubleCapability.clone();
        mIntCapability = intCapability.clone();
        mFillLightModeArray = fillLightModeArray == null ? null : fillLightModeArray.clone();
        mMeteringMode = meteringMode.clone();
        mMeteringModeArray = new int[MeteringModeType.NUM_ENTRIES][];
        for (int i = 0; i < meteringModeArray.length; i++) {
            mMeteringModeArray[i] =
                    meteringModeArray[i] == null ? null : meteringModeArray[i].clone();
        }
    }

    @CalledByNative
    public boolean getBool(@PhotoCapabilityBool int capability) {
        if (capability < 0 || capability >= PhotoCapabilityBool.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        return mBoolCapability[capability];
    }

    @CalledByNative
    public double getDouble(@PhotoCapabilityDouble int capability) {
        if (capability < 0 || capability >= PhotoCapabilityDouble.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        return mDoubleCapability[capability];
    }

    @CalledByNative
    public int getInt(@PhotoCapabilityInt int capability) {
        if (capability < 0 || capability >= PhotoCapabilityInt.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        return mIntCapability[capability];
    }

    @CalledByNative
    public int[] getFillLightModeArray() {
        assert AndroidFillLightMode.NOT_SET == 0;
        return mFillLightModeArray != null ? mFillLightModeArray.clone() : new int[0];
    }

    @CalledByNative
    public @AndroidMeteringMode int getMeteringMode(@MeteringModeType int type) {
        if (type < 0 || type >= MeteringModeType.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        return mMeteringMode[type];
    }

    @CalledByNative
    public int[] getMeteringModeArray(@MeteringModeType int type) {
        if (type < 0 || type >= MeteringModeType.NUM_ENTRIES) {
            throw new IllegalArgumentException();
        }
        assert AndroidMeteringMode.NOT_SET == 0;
        return mMeteringModeArray[type] != null ? mMeteringModeArray[type].clone() : new int[0];
    }

    public static class Builder {
        public boolean mBoolCapability[] = new boolean[PhotoCapabilityBool.NUM_ENTRIES];
        public double mDoubleCapability[] = new double[PhotoCapabilityDouble.NUM_ENTRIES];
        public int mIntCapability[] = new int[PhotoCapabilityInt.NUM_ENTRIES];
        public int mFillLightModeArray[];
        public int mMeteringMode[] = new int[MeteringModeType.NUM_ENTRIES];
        public int mMeteringModeArray[][] = new int[MeteringModeType.NUM_ENTRIES][];

        public Builder() {}

        public Builder setBool(@PhotoCapabilityBool int capability, boolean value) {
            this.mBoolCapability[capability] = value;
            return this;
        }

        public Builder setDouble(@PhotoCapabilityDouble int capability, double value) {
            this.mDoubleCapability[capability] = value;
            return this;
        }

        public Builder setInt(@PhotoCapabilityInt int capability, int value) {
            this.mIntCapability[capability] = value;
            return this;
        }

        public Builder setFillLightModeArray(int[] value) {
            this.mFillLightModeArray = value.clone();
            return this;
        }

        public Builder setMeteringMode(@MeteringModeType int type, int value) {
            this.mMeteringMode[type] = value;
            return this;
        }

        public Builder setMeteringModeArray(@MeteringModeType int type, int[] value) {
            this.mMeteringModeArray[type] = value.clone();
            return this;
        }

        public PhotoCapabilities build() {
            return new PhotoCapabilities(
                    mBoolCapability,
                    mDoubleCapability,
                    mIntCapability,
                    mFillLightModeArray,
                    mMeteringMode,
                    mMeteringModeArray);
        }
    }
}
