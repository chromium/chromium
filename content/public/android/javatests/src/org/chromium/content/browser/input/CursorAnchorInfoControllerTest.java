// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.graphics.Matrix;
import android.graphics.RectF;
import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.view.inputmethod.CursorAnchorInfo;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureList;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.test.ContentJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;

import java.util.Map;

/** Test for {@link CursorAnchorInfoController}. */
@RunWith(ContentJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class CursorAnchorInfoControllerTest {
    private static final class TestViewDelegate implements CursorAnchorInfoController.ViewDelegate {
        public int locationX;
        public int locationY;

        @Override
        public void getLocationOnScreen(View view, int[] location) {
            location[0] = locationX;
            location[1] = locationY;
        }
    }

    private static final class TestComposingTextDelegate
            implements CursorAnchorInfoController.ComposingTextDelegate {
        private String mText;
        private int mSelectionStart = -1;
        private int mSelectionEnd = -1;
        private int mComposingTextStart = -1;
        private int mComposingTextEnd = -1;

        @Override
        public CharSequence getText() {
            return mText;
        }

        @Override
        public int getSelectionStart() {
            return mSelectionStart;
        }

        @Override
        public int getSelectionEnd() {
            return mSelectionEnd;
        }

        @Override
        public int getComposingTextStart() {
            return mComposingTextStart;
        }

        @Override
        public int getComposingTextEnd() {
            return mComposingTextEnd;
        }

        public void updateTextAndSelection(
                CursorAnchorInfoController controller,
                String text,
                int compositionStart,
                int compositionEnd,
                int selectionStart,
                int selectionEnd) {
            mText = text;
            mSelectionStart = selectionStart;
            mSelectionEnd = selectionEnd;
            mComposingTextStart = compositionStart;
            mComposingTextEnd = compositionEnd;
            controller.invalidateLastCursorAnchorInfo();
        }

        public void clearTextAndSelection(CursorAnchorInfoController controller) {
            updateTextAndSelection(controller, null, -1, -1, -1, -1);
        }
    }

    private static class AssertionHelper {
        static void assertScaleAndTranslate(
                float expectedScale,
                float expectedTranslateX,
                float expectedTranslateY,
                CursorAnchorInfo actual) {
            Matrix expectedMatrix = new Matrix();
            expectedMatrix.setScale(expectedScale, expectedScale);
            expectedMatrix.postTranslate(expectedTranslateX, expectedTranslateY);
            Assert.assertEquals(expectedMatrix, actual.getMatrix());
        }

        static void assertHasInsertionMarker(
                int expectedFlags,
                float expectedHorizontal,
                float expectedTop,
                float expectedBaseline,
                float expectedBottom,
                CursorAnchorInfo actual) {
            Assert.assertEquals(expectedFlags, actual.getInsertionMarkerFlags());
            Assert.assertEquals(expectedHorizontal, actual.getInsertionMarkerHorizontal(), 0);
            Assert.assertEquals(expectedTop, actual.getInsertionMarkerTop(), 0);
            Assert.assertEquals(expectedBaseline, actual.getInsertionMarkerBaseline(), 0);
            Assert.assertEquals(expectedBottom, actual.getInsertionMarkerBottom(), 0);
        }

        static void assertHasNoInsertionMarker(CursorAnchorInfo actual) {
            Assert.assertEquals(0, actual.getInsertionMarkerFlags());
            Assert.assertTrue(Float.isNaN(actual.getInsertionMarkerHorizontal()));
            Assert.assertTrue(Float.isNaN(actual.getInsertionMarkerTop()));
            Assert.assertTrue(Float.isNaN(actual.getInsertionMarkerBaseline()));
            Assert.assertTrue(Float.isNaN(actual.getInsertionMarkerBottom()));
        }

        static void assertComposingText(
                CharSequence expectedComposingText,
                int expectedComposingTextStart,
                CursorAnchorInfo actual) {
            Assert.assertTrue(TextUtils.equals(expectedComposingText, actual.getComposingText()));
            Assert.assertEquals(expectedComposingTextStart, actual.getComposingTextStart());
        }

        static void assertSelection(
                int expecteSelectionStart, int expecteSelectionEnd, CursorAnchorInfo actual) {
            Assert.assertEquals(expecteSelectionStart, actual.getSelectionStart());
            Assert.assertEquals(expecteSelectionEnd, actual.getSelectionEnd());
        }
    }

    @Before
    public void setUp() {
        // Cannot access native features exposed to Java in tests without native initialization.
        // Instead, assign a test value to the feature.
        FeatureList.setTestFeatures(Map.of(BlinkFeatures.CURSOR_ANCHOR_INFO_MOJO_PIPE, false));
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testFocusedNodeChanged() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;

        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        Assert.assertFalse(
                "IC#onRequestCursorUpdates() must be rejected if the focused node is not editable.",
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));

        // Make sure that the focused node is considered to be non-editable by default.
        controller.setBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, null, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());

        controller.focusedNodeChanged(false);
        composingTextDelegate.clearTextAndSelection(controller);

        // Make sure that the controller does not crash even if it is called while the focused node
        // is not editable.
        controller.setBounds(new float[] {30.0f, 1.0f, 32.0f, 3.0f}, null, view);
        composingTextDelegate.updateTextAndSelection(controller, "1", 0, 1, 0, 1);
        controller.onUpdateFrameInfo(1.0f, 100.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testImmediateMode() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);

        // Make sure that #updateCursorAnchorInfo() is not be called until the matrix info becomes
        // available with #onUpdateFrameInfo().
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        true /* immediate request */, false /* monitor request */, view));
        controller.setBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, null, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        Assert.assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that 2nd call of #onUpdateFrameInfo() is ignored.
        controller.onUpdateFrameInfo(2.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that #onUpdateFrameInfo() is immediately called because the matrix info is
        // already available.
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        true /* immediate request */, false /* monitor request */, view));
        Assert.assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that CURSOR_UPDATE_IMMEDIATE and CURSOR_UPDATE_MONITOR can be specified at
        // the same time.
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        true /* immediate request*/, true /* monitor request */, view));
        Assert.assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that CURSOR_UPDATE_IMMEDIATE is cleared if the focused node becomes
        // non-editable.
        controller.focusedNodeChanged(false);
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        true /* immediate request */, false /* monitor request */, view));
        controller.focusedNodeChanged(false);
        composingTextDelegate.clearTextAndSelection(controller);
        controller.onUpdateFrameInfo(1.0f, 100.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that CURSOR_UPDATE_IMMEDIATE can be enabled again.
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        true /* immediate request */, false /* monitor request */, view));
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText(null, -1, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(-1, -1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testMonitorMode() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);

        // Make sure that #updateCursorAnchorInfo() is not be called until the matrix info becomes
        // available with #onUpdateFrameInfo().
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));
        controller.setBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, null, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        Assert.assertEquals(0, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is not be called if any coordinate parameter is
        // changed for better performance.
        controller.setBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, null, view);
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that #updateCursorAnchorInfo() is called if #setBounds()
        // is called with a different parameter.
        controller.setBounds(new float[] {30.0f, 1.0f, 32.0f, 3.0f}, null, view);
        Assert.assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is called if #updateTextAndSelection()
        // is called with a different parameter.
        composingTextDelegate.updateTextAndSelection(controller, "1", 0, 1, 0, 1);
        Assert.assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("1", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is called if #onUpdateFrameInfo()
        // is called with a different parameter.
        controller.onUpdateFrameInfo(2.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("1", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that #updateCursorAnchorInfo() is called when the view origin is changed.
        viewDelegate.locationX = 7;
        viewDelegate.locationY = 9;
        controller.onUpdateFrameInfo(2.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(2.0f, 7.0f, 9.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(30.0f, 1.0f, 32.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("1", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Make sure that CURSOR_UPDATE_IMMEDIATE is cleared if the focused node becomes
        // non-editable.
        controller.focusedNodeChanged(false);
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));
        controller.focusedNodeChanged(false);
        composingTextDelegate.clearTextAndSelection(controller);
        controller.setBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, null, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());

        // Make sure that CURSOR_UPDATE_MONITOR can be enabled again.
        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));
        controller.setBounds(new float[] {0.0f, 1.0f, 2.0f, 3.0f}, null, view);
        composingTextDelegate.updateTextAndSelection(controller, "0", 0, 1, 0, 1);
        Assert.assertEquals(5, immw.getUpdateCursorAnchorInfoCounter());
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 2.0f, 0.0f, 3.0f, view);
        Assert.assertEquals(6, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                2.0f,
                0.0f,
                3.0f,
                3.0f,
                immw.getLastCursorAnchorInfo());
        Assert.assertEquals(
                new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        AssertionHelper.assertComposingText("0", 0, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(0, 1, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testSetBounds() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;

        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));

        composingTextDelegate.updateTextAndSelection(controller, "01234", 1, 3, 1, 1);
        controller.setBounds(
                new float[] {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 1.1f, 6.0f, 2.9f},
                new float[] {0.0f, 1.0f, 6.0f, 2.9f},
                view);
        controller.onUpdateFrameInfo(
                1.0f, 0.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        // Expect null at position 0 as composition starts from position 1.
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        Assert.assertEquals(
                new RectF(0.0f, 1.0f, 2.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(1));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(1));
        // TODO(crbug.com/40940885): Replace these values and the ones below with the original
        //  floats once we support RectF objects from Blink.
        Assert.assertEquals(
                new RectF(4.0f, 1.0f, 6.0f, 3.0f),
                immw.getLastCursorAnchorInfo().getCharacterBounds(2));
        Assert.assertEquals(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(2));
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(3));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(3));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            Assert.assertEquals(1, immw.getLastCursorAnchorInfo().getVisibleLineBounds().size());
            Assert.assertEquals(
                    new RectF(0.0f, 1.0f, 6.0f, 3.0f),
                    immw.getLastCursorAnchorInfo().getVisibleLineBounds().get(0));
        }
        AssertionHelper.assertComposingText("12", 1, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(1, 1, immw.getLastCursorAnchorInfo());
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testUpdateTextAndSelection() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;

        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));

        composingTextDelegate.updateTextAndSelection(controller, "01234", 3, 3, 1, 1);
        controller.onUpdateFrameInfo(
                1.0f, 0.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(0));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(0));
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(1));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(1));
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(2));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(2));
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(3));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(3));
        Assert.assertEquals(null, immw.getLastCursorAnchorInfo().getCharacterBounds(4));
        Assert.assertEquals(0, immw.getLastCursorAnchorInfo().getCharacterBoundsFlags(4));
        AssertionHelper.assertComposingText("", 3, immw.getLastCursorAnchorInfo());
        AssertionHelper.assertSelection(1, 1, immw.getLastCursorAnchorInfo());
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testInsertionMarker() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));

        // Test no insertion marker.
        controller.onUpdateFrameInfo(
                1.0f, 0.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertHasNoInsertionMarker(immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Test a visible insertion marker.
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, true, 10.0f, 23.0f, 29.0f, view);
        Assert.assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_VISIBLE_REGION,
                10.0f,
                23.0f,
                29.0f,
                29.0f,
                immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // Test a invisible insertion marker.
        controller.onUpdateFrameInfo(1.0f, 0.0f, true, false, 10.0f, 23.0f, 29.0f, view);
        Assert.assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertHasInsertionMarker(
                CursorAnchorInfo.FLAG_HAS_INVISIBLE_REGION,
                10.0f,
                23.0f,
                29.0f,
                29.0f,
                immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }

    @Test
    @SmallTest
    @Feature({"Input-Text-IME"})
    public void testMatrix() {
        TestInputMethodManagerWrapper immw = new TestInputMethodManagerWrapper(null);
        TestViewDelegate viewDelegate = new TestViewDelegate();
        TestComposingTextDelegate composingTextDelegate = new TestComposingTextDelegate();
        CursorAnchorInfoController controller =
                CursorAnchorInfoController.createForTest(immw, composingTextDelegate, viewDelegate);
        View view = null;

        controller.focusedNodeChanged(true);
        composingTextDelegate.clearTextAndSelection(controller);
        Assert.assertTrue(
                controller.onRequestCursorUpdates(
                        false /* immediate request */, true /* monitor request */, view));

        // Test no transformation
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;
        controller.onUpdateFrameInfo(
                1.0f, 0.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(1, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(1.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // device scale factor == 2.0
        viewDelegate.locationX = 0;
        viewDelegate.locationY = 0;
        controller.onUpdateFrameInfo(
                2.0f, 0.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(2, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(2.0f, 0.0f, 0.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // device scale factor == 2.0
        // view origin == (10, 141)
        viewDelegate.locationX = 10;
        viewDelegate.locationY = 141;
        controller.onUpdateFrameInfo(
                2.0f, 0.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(3, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(
                2.0f, 10.0f, 141.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();

        // device scale factor == 2.0
        // content offset Y = 40.0f
        // view origin == (10, 141)
        viewDelegate.locationX = 10;
        viewDelegate.locationY = 141;
        controller.onUpdateFrameInfo(
                2.0f, 40.0f, false, false, Float.NaN, Float.NaN, Float.NaN, view);
        Assert.assertEquals(4, immw.getUpdateCursorAnchorInfoCounter());
        AssertionHelper.assertScaleAndTranslate(
                2.0f, 10.0f, 181.0f, immw.getLastCursorAnchorInfo());
        immw.clearLastCursorAnchorInfo();
    }
}
