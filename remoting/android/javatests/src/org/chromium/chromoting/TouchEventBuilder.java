// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

import org.chromium.chromoting.jni.TouchEventData;

import java.util.ArrayList;

/** A helper class to build a {@link MockInputStub.TouchEvent}. */
public final class TouchEventBuilder {
    private final ArrayList<TouchEventData> mData;
    private @TouchEventData.EventType int mEventType;

    // Following fields are the of the pending TouchEventData. They will be added to {@link #data}
    // by calling appendData().
    private int mId;
    private float mX;
    private float mY;
    private float mRadiusX;
    private float mRadiusY;
    private float mAngleInRadians;
    private float mPressure;

    public TouchEventBuilder() {
        mData = new ArrayList<>();
        clear();
    }

    public TouchEventBuilder withEventType(@TouchEventData.EventType int eventType) {
        mEventType = eventType;
        return this;
    }

    public TouchEventBuilder withId(int id) {
        mId = id;
        return this;
    }

    public TouchEventBuilder withX(float x) {
        mX = x;
        return this;
    }

    public TouchEventBuilder withY(float y) {
        mY = y;
        return this;
    }

    public TouchEventBuilder withRadiusX(float radiusX) {
        mRadiusX = radiusX;
        return this;
    }

    public TouchEventBuilder withRadiusY(float radiusY) {
        mRadiusY = radiusY;
        return this;
    }

    public TouchEventBuilder withAngleInRadians(float angleInRadians) {
        mAngleInRadians = angleInRadians;
        return this;
    }

    public TouchEventBuilder withPressure(float pressure) {
        mPressure = pressure;
        return this;
    }

    public TouchEventBuilder append() {
        mData.add(new TouchEventData(mId, mX, mY, mRadiusX, mRadiusY, mAngleInRadians, mPressure));
        resetPending();
        return this;
    }

    public MockInputStub.TouchEvent build() {
        return new MockInputStub.TouchEvent(mEventType, mData.toArray(new TouchEventData[] {}));
    }

    private void clear() {
        mEventType = TouchEventData.EventType.UNKNOWN;
        mData.clear();
        resetPending();
    }

    private void resetPending() {
        mId = MockInputStub.TouchEvent.INVALID_ID;
        mX = MockInputStub.TouchEvent.INVALID_POSITION;
        mY = MockInputStub.TouchEvent.INVALID_POSITION;
        mRadiusX = MockInputStub.TouchEvent.INVALID_POSITION;
        mRadiusY = MockInputStub.TouchEvent.INVALID_POSITION;
        mAngleInRadians = MockInputStub.TouchEvent.INVALID_RADIANS;
        mPressure = MockInputStub.TouchEvent.INVALID_POSITION;
    }
}
