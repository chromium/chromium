// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import static org.chromium.ui.listmenu.ListItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;
import android.os.Build;
import android.provider.Settings;
import android.view.ActionMode;
import android.view.Menu;
import android.view.ViewGroup;
import android.view.textclassifier.TextClassification;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.fakes.RoboMenu;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.PopupController;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.RenderWidgetHostViewImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ActionModeCallback;
import org.chromium.content_public.browser.PendingSelectionMenu;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionEventProcessor;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionMenuItem.ItemGroupOffset;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.content_public.browser.test.util.TestSelectionDropdownMenuDelegate;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuSubmenuItemProperties;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.mojom.MenuSourceType;
import org.chromium.ui.touch_selection.SelectionEventType;
import org.chromium.ui.touch_selection.TouchSelectionDraggableType;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;

/** Unit tests for {@link SelectionPopupController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SelectionPopupControllerTest {
    private MenuModelBridge mMenuModelBridge;
    private SelectionPopupControllerImpl mController;
    private Context mContext;
    private WeakReference<Context> mWeakContext;
    private TypedArray mTypedArray;
    private WindowAndroid mWindowAndroid;
    private WebContentsImpl mWebContents;
    private ViewGroup mView;
    private ViewAndroidDelegate mViewAndroidDelegate;
    private ActionMode mActionMode;
    private PackageManager mPackageManager;
    private SmartSelectionEventProcessor mLogger;
    private RenderWidgetHostViewImpl mRenderWidgetHostViewImpl;
    private RenderCoordinatesImpl mRenderCoordinates;
    private ContentResolver mContentResolver;
    private PopupController mPopupController;
    private GestureListenerManagerImpl mGestureStateListenerManager;
    private RenderFrameHost mRenderFrameHost;
    private ActionModeCallback mActionModeCallback;

    private static final String MOUNTAIN_FULL = "585 Franklin Street, Mountain View, CA 94041";
    private static final String MOUNTAIN = "Mountain";
    private static final String AMPHITHEATRE_FULL = "1600 Amphitheatre Parkway";
    private static final String AMPHITHEATRE = "Amphitheatre";
    private static final String EXTRA_MENU_ITEM_TITLE = "Extra Menu Item Title";

    private static class TestSelectionClient implements SelectionClient {
        private SelectionClient.Result mResult;
        private SelectionClient.ResultCallback mResultCallback;
        private SmartSelectionEventProcessor mLogger;

        @Override
        public void onSelectionChanged(String selection) {}

        @Override
        public void onSelectionEvent(int eventType, float posXPix, float poxYPix) {}

        @Override
        public void selectAroundCaretAck(SelectAroundCaretResult result) {}

        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            mResultCallback.onClassified(mResult);
            return true;
        }

        @Override
        public void cancelAllRequests() {}

        @Override
        public SelectionEventProcessor getSelectionEventProcessor() {
            return mLogger;
        }

        public void setResult(SelectionClient.Result result) {
            mResult = result;
        }

        public void setResultCallback(SelectionClient.ResultCallback callback) {
            mResultCallback = callback;
        }

        public void setLogger(SmartSelectionEventProcessor logger) {
            mLogger = logger;
        }
    }

    private static class SelectionClientOnlyReturnTrue extends TestSelectionClient {
        @Override
        public boolean requestSelectionPopupUpdates(boolean shouldSuggest) {
            return true;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowLog.stream = System.out;

        mContext = Mockito.mock(Context.class);
        mWeakContext = new WeakReference<Context>(mContext);
        mTypedArray = Mockito.mock(TypedArray.class);
        mWindowAndroid = Mockito.mock(WindowAndroid.class);
        mWebContents = Mockito.mock(WebContentsImpl.class);
        mView = Mockito.mock(ViewGroup.class);
        mViewAndroidDelegate = ViewAndroidDelegate.createBasicDelegate(mView);
        mActionMode = Mockito.mock(ActionMode.class);
        mPackageManager = Mockito.mock(PackageManager.class);
        mRenderWidgetHostViewImpl = Mockito.mock(RenderWidgetHostViewImpl.class);
        mRenderFrameHost = Mockito.mock(RenderFrameHost.class);
        mRenderCoordinates = Mockito.mock(RenderCoordinatesImpl.class);
        mLogger = Mockito.mock(SmartSelectionEventProcessor.class);
        mPopupController = Mockito.mock(PopupController.class);
        mGestureStateListenerManager = Mockito.mock(GestureListenerManagerImpl.class);
        mMenuModelBridge = Mockito.mock(MenuModelBridge.class);
        mActionModeCallback = Mockito.mock(ActionModeCallback.class);

        setDropdownMenuFeatureEnabled(false);

        SelectionPopupControllerImpl.setDisableMagnifierForTesting(true);

        mContentResolver = RuntimeEnvironment.application.getContentResolver();
        // To let isDeviceProvisioned() call in showSelectionMenu() return true.
        Settings.Global.putInt(mContentResolver, Settings.Global.DEVICE_PROVISIONED, 1);

        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mContext.getContentResolver()).thenReturn(mContentResolver);
        when(mContext.obtainStyledAttributes(Mockito.any(int[].class))).thenReturn(mTypedArray);
        when(mWebContents.getRenderWidgetHostView()).thenReturn(mRenderWidgetHostViewImpl);
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getDeviceScaleFactor()).thenReturn(1.f);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);
        when(mWebContents.getContext()).thenReturn(mContext);
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        when(mGestureStateListenerManager.isScrollInProgress()).thenReturn(false);
        when(mWindowAndroid.getContext()).thenReturn(mWeakContext);
        when(mMenuModelBridge.getListItems()).thenReturn(List.of());

        mController = SelectionPopupControllerImpl.createForTesting(mWebContents, mPopupController);
        GestureListenerManagerImpl.setInstanceForTesting(mGestureStateListenerManager);
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionAdjustSelectionRange() {
        InOrder order = inOrder(mWebContents, mView);
        SelectionClient.Result result = resultForAmphitheatre();

        // Setup SelectionClient for SelectionPopupController.
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mController.getResultCallback());
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                mController,
                AMPHITHEATRE,
                /* selectionStartOffset= */ 5,
                MenuSourceType.LONG_PRESS);

        // adjustSelectionByCharacterOffset() should be called.
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(result.startAdjust, result.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        when(mView.startActionMode(any(), anyInt())).thenReturn(mActionMode);

        // Call showSelectionMenu again, which is adjustSelectionByCharacterOffset triggered.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.ADJUST_SELECTION);

        order.verify(mView).startActionMode(isNull(), eq(ActionMode.TYPE_FLOATING));

        SelectionClient.Result returnResult = mController.getClassificationResult();
        assertEquals(-5, returnResult.startAdjust);
        assertEquals(8, returnResult.endAdjust);
        assertEquals("Maps", returnResult.textClassification.getActions().get(0).getTitle());

        assertTrue(mController.isActionModeValid());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionAnotherLongPressAfterAdjustment() {
        InOrder order = inOrder(mWebContents, mView);
        SelectionClient.Result result = resultForAmphitheatre();
        SelectionClient.Result newResult = resultForMountain();

        // Set SelectionClient for SelectionPopupController.
        TestSelectionClient client = new TestSelectionClient();
        client.setResult(result);
        client.setResultCallback(mController.getResultCallback());
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                mController,
                AMPHITHEATRE,
                /* selectionStartOffset= */ 5,
                MenuSourceType.LONG_PRESS);

        // adjustSelectionByCharacterOffset() should be called.
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(result.startAdjust, result.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        // Another long press triggered showSelectionMenu() call.
        client.setResult(newResult);
        showSelectionMenu(
                mController, MOUNTAIN, /* selectionStartOffset= */ 21, MenuSourceType.LONG_PRESS);
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(newResult.startAdjust, newResult.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        when(mView.startActionMode(any(), anyInt())).thenReturn(mActionMode);

        // First adjustSelectionByCharacterOffset() triggered.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.ADJUST_SELECTION);

        SelectionClient.Result returnResult = mController.getClassificationResult();
        assertEquals(-21, returnResult.startAdjust);
        assertEquals(15, returnResult.endAdjust);
        assertEquals("Maps", returnResult.textClassification.getActions().get(0).getTitle());

        // Second adjustSelectionByCharacterOffset() triggered.
        showSelectionMenu(
                mController,
                MOUNTAIN_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.ADJUST_SELECTION);

        order.verify(mView).startActionMode(isNull(), eq(ActionMode.TYPE_FLOATING));
        assertTrue(mController.isActionModeValid());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionAnotherLongPressBeforeAdjustment() {
        InOrder order = inOrder(mWebContents, mView);
        SelectionClient.Result result = resultForAmphitheatre();
        SelectionClient.Result newResult = resultForMountain();

        // This client won't call SmartSelectionCallback.
        TestSelectionClient client = new SelectionClientOnlyReturnTrue();
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                mController,
                AMPHITHEATRE,
                /* selectionStartOffset= */ 5,
                MenuSourceType.LONG_PRESS);

        // Another long press triggered showSelectionMenu() call.
        showSelectionMenu(
                mController, MOUNTAIN, /* selectionStartOffset= */ 21, MenuSourceType.LONG_PRESS);

        // Then we are done with the first classification.
        mController.getResultCallback().onClassified(result);

        // Followed by the second classifaction.
        mController.getResultCallback().onClassified(newResult);

        // adjustSelectionByCharacterOffset() should be called.
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(result.startAdjust, result.endAdjust, true);
        order.verify(mWebContents)
                .adjustSelectionByCharacterOffset(newResult.startAdjust, newResult.endAdjust, true);
        assertFalse(mController.isActionModeValid());

        when(mView.startActionMode(any(), anyInt())).thenReturn(mActionMode);

        // First adjustSelectionByCharacterOffset() triggered.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.ADJUST_SELECTION);

        SelectionClient.Result returnResult = mController.getClassificationResult();
        assertEquals(-21, returnResult.startAdjust);
        assertEquals(15, returnResult.endAdjust);
        assertEquals("Maps", returnResult.textClassification.getActions().get(0).getTitle());

        // Second adjustSelectionByCharacterOffset() triggered.
        showSelectionMenu(
                mController,
                MOUNTAIN_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.ADJUST_SELECTION);

        order.verify(mView).startActionMode(isNull(), eq(ActionMode.TYPE_FLOATING));
        assertTrue(mController.isActionModeValid());
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionLoggingExpansion() {
        InOrder order = inOrder(mLogger);
        SelectionClient.Result result = resultForAmphitheatre();

        // Setup SelectionClient for SelectionPopupController.
        TestSelectionClient client = new SelectionClientOnlyReturnTrue();
        client.setLogger(mLogger);
        client.setResult(result);
        client.setResultCallback(mController.getResultCallback());
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                mController,
                AMPHITHEATRE,
                /* selectionStartOffset= */ 5,
                MenuSourceType.LONG_PRESS);

        when(mView.startActionMode(any(), anyInt())).thenReturn(mActionMode);

        order.verify(mLogger).onSelectionStarted(AMPHITHEATRE, 5, true);

        mController.getResultCallback().onClassified(result);

        // Call showSelectionMenu again, which is adjustSelectionByCharacterOffset triggered.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.ADJUST_SELECTION);

        order.verify(mLogger)
                .onSelectionModified(
                        eq(AMPHITHEATRE_FULL), eq(0), isA(SelectionClient.Result.class));

        // Dragging selection handle, select "1600 Amphitheatre".
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL.substring(0, 17),
                /* selectionStartOffset= */ 0,
                MenuSourceType.TOUCH_HANDLE);

        order.verify(mLogger, never())
                .onSelectionModified(anyString(), anyInt(), any(SelectionClient.Result.class));

        mController.getResultCallback().onClassified(resultForNoChange());

        order.verify(mLogger)
                .onSelectionModified(
                        eq("1600 Amphitheatre"), eq(0), isA(SelectionClient.Result.class));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testSmartSelectionLoggingNoExpansion() {
        InOrder order = inOrder(mLogger);
        SelectionClient.Result result = resultForNoChange();

        // Setup SelectionClient for SelectionPopupController.
        TestSelectionClient client = new SelectionClientOnlyReturnTrue();
        client.setLogger(mLogger);
        client.setResult(result);
        client.setResultCallback(mController.getResultCallback());
        mController.setSelectionClient(client);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                mController,
                AMPHITHEATRE,
                /* selectionStartOffset= */ 5,
                MenuSourceType.LONG_PRESS);

        when(mView.startActionMode(any(ActionMode.Callback2.class), anyInt()))
                .thenReturn(mActionMode);
        order.verify(mLogger).onSelectionStarted(AMPHITHEATRE, 5, true);

        // No expansion.
        mController.getResultCallback().onClassified(result);
        order.verify(mLogger)
                .onSelectionModified(eq(AMPHITHEATRE), eq(5), any(SelectionClient.Result.class));

        // Dragging selection handle, select "1600 Amphitheatre".
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL.substring(0, 17),
                /* selectionStartOffset= */ 0,
                MenuSourceType.TOUCH_HANDLE);

        order.verify(mLogger, never())
                .onSelectionModified(anyString(), anyInt(), any(SelectionClient.Result.class));
        mController.getResultCallback().onClassified(resultForNoChange());
        order.verify(mLogger)
                .onSelectionModified(
                        eq("1600 Amphitheatre"), eq(0), isA(SelectionClient.Result.class));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testBlockSelectionClientWhenUnprovisioned() {
        // Device is not provisioned.
        Settings.Global.putInt(mContentResolver, Settings.Global.DEVICE_PROVISIONED, 0);

        assertNull(
                SmartSelectionClient.fromWebContents(
                        mController.getResultCallback(), mWebContents));
    }

    @Test
    @Feature({"TextInput", "SmartSelection"})
    public void testBlockSelectionClientWhenIncognito() {
        // Incognito.
        when(mWebContents.isIncognito()).thenReturn(true);

        assertNull(
                SmartSelectionClient.fromWebContents(
                        mController.getResultCallback(), mWebContents));
    }

    @Test
    @Feature({"TextInput", "Magnifier"})
    public void testHandleObserverSelectionHandle() {
        MagnifierAnimator magnifierAnimator = Mockito.mock(MagnifierAnimator.class);
        InOrder order = inOrder(magnifierAnimator);
        mController.setMagnifierAnimator(magnifierAnimator);

        // Selection handles shown.
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_SHOWN, 0, 0, 1, 1);

        // Selection handles drag started.
        mController.onDragUpdate(TouchSelectionDraggableType.TOUCH_HANDLE, 0.f, 0.f);
        order.verify(magnifierAnimator).handleDragStartedOrMoved(0.f, 0.f);

        // Moving.
        mController.onDragUpdate(TouchSelectionDraggableType.TOUCH_HANDLE, 5.f, 5.f);
        order.verify(magnifierAnimator).handleDragStartedOrMoved(5.f, 5.f);

        // Selection handle drag stopped.
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 0, 0, 1, 1);
        order.verify(magnifierAnimator).handleDragStopped();
    }

    @Test
    @Feature({"TextInput", "Magnifier"})
    public void testHandleObserverInsertionHandle() {
        MagnifierAnimator magnifierAnimator = Mockito.mock(MagnifierAnimator.class);
        InOrder order = inOrder(magnifierAnimator);
        mController.setMagnifierAnimator(magnifierAnimator);

        // Insertion handle shown.
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_SHOWN, 0, 0, 1, 1);

        // Insertion handle drag started.
        mController.onDragUpdate(TouchSelectionDraggableType.TOUCH_HANDLE, 0.f, 0.f);
        order.verify(magnifierAnimator).handleDragStartedOrMoved(0.f, 0.f);

        // Moving.
        mController.onDragUpdate(TouchSelectionDraggableType.TOUCH_HANDLE, 5.f, 5.f);
        order.verify(magnifierAnimator).handleDragStartedOrMoved(5.f, 5.f);

        // Insertion handle drag stopped.
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_DRAG_STOPPED, 0, 0, 1, 1);
        order.verify(magnifierAnimator).handleDragStopped();
    }

    @Test
    @Feature({"TextInput", "HandleHapticFeedback"})
    public void testInsertionHandleHapticFeedback() {
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        spyController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_MOVED, 0, 0, 1, 1);
        // Any INSERTION_HANDLE_MOVED before INSERTION_HANDLE_DRAG_STARTED should not trigger haptic
        // feedback.
        Mockito.verify(spyController, never()).performHapticFeedback();

        spyController.onSelectionEvent(
                SelectionEventType.INSERTION_HANDLE_DRAG_STARTED, 0, 0, 1, 1);
        spyController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_MOVED, 0, 0, 1, 1);
        spyController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_MOVED, 0, 0, 1, 1);
        spyController.onSelectionEvent(
                SelectionEventType.INSERTION_HANDLE_DRAG_STOPPED, 0, 0, 1, 1);

        // We called twice.
        Mockito.verify(spyController, times(2)).performHapticFeedback();

        spyController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_MOVED, 0, 0, 1, 1);
        // Any INSERTION_HANDLE_MOVED after INSERTION_HANDLE_DRAG_STOPPED should not trigger more
        // haptic feedback.
        Mockito.verify(spyController, times(2)).performHapticFeedback();
    }

    @Test
    @Feature({"TextInput", "HandleHapticFeedback"})
    public void testSelectionHandleHapticFeedback() {
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        spyController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_MOVED, 0, 0, 1, 1);
        // Any SELECTION_HANDLES_MOVED before SELECTION_HANDLE_DRAG_STARTED should not trigger
        // haptic feedback.
        Mockito.verify(spyController, never()).performHapticFeedback();

        spyController.onSelectionEvent(
                SelectionEventType.SELECTION_HANDLE_DRAG_STARTED, 0, 0, 1, 1);
        spyController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_MOVED, 0, 0, 1, 1);
        spyController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_MOVED, 0, 0, 1, 1);
        spyController.onSelectionEvent(
                SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 0, 0, 1, 1);

        // We called twice.
        Mockito.verify(spyController, times(2)).performHapticFeedback();

        spyController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_MOVED, 0, 0, 1, 1);
        // Any SELECTION_HANDLES_MOVED after SELECTION_HANDLE_DRAG_STOPPED should not trigger more
        // haptic feedback.
        Mockito.verify(spyController, times(2)).performHapticFeedback();
    }

    @Test
    @Feature({"TextInput"})
    public void testSelectionWhenUnselectAndFocusedNodeChanged() {
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);

        when(mView.startActionMode(any(), anyInt())).thenReturn(mActionMode);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                spyController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.LONG_PRESS);

        Mockito.verify(mView).startActionMode(isNull(), eq(ActionMode.TYPE_FLOATING));
        // showSelectionMenu() will invoke the first call to finishActionMode() in the
        // showActionModeOrClearOnFailure().
        Mockito.verify(spyController, times(1)).finishActionMode();
        assertTrue(spyController.isSelectActionBarShowing());
        assertTrue(spyController.isSelectActionBarShowingSupplier().get());

        // Clear the selected text.
        spyController.onSelectionChanged("");
        // Changed the focused node attribute to non-editable and non-password.
        spyController.updateSelectionState(false, false);

        // finishActionMode will be called twice for unselect.
        Mockito.verify(spyController, times(2)).finishActionMode();

        // SELECTION_HANDLES_CLEARED happens.
        spyController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_CLEARED, 0, 0, 1, 1);

        assertFalse(spyController.isSelectActionBarShowing());
        assertFalse(spyController.isSelectActionBarShowingSupplier().get());
        Mockito.verify(spyController, times(3)).finishActionMode();
    }

    @Test
    @Feature({"TextInput"})
    public void testSelectionWhenWindowIsNull() {
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);

        when(mView.startActionMode(any(), anyInt())).thenReturn(mActionMode);

        // Long press triggered showSelectionMenu() call.
        showSelectionMenu(
                spyController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.LONG_PRESS);

        Mockito.verify(mView).startActionMode(isNull(), eq(ActionMode.TYPE_FLOATING));
        // showSelectionMenu() will invoke the first call to finishActionMode() in the
        // showActionModeOrClearOnFailure().
        Mockito.verify(spyController, times(1)).finishActionMode();
        assertTrue(spyController.isSelectActionBarShowing());
        assertTrue(spyController.isSelectActionBarShowingSupplier().get());

        // Setting the window to null should clear selections and reset the state.
        spyController.onWindowAndroidChanged(null);
        // Changed the focused node attribute to non-editable and non-password.
        spyController.updateSelectionState(false, false);

        // finishActionMode will be called twice for unselect.
        Mockito.verify(spyController, times(2)).finishActionMode();

        // SELECTION_HANDLES_CLEARED happens.
        spyController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_CLEARED, 0, 0, 1, 1);

        assertFalse(spyController.isSelectActionBarShowing());
        assertFalse(spyController.isSelectActionBarShowingSupplier().get());
        Mockito.verify(spyController, times(3)).finishActionMode();
    }

    @Test
    @Feature({"TextInput"})
    public void testProcessTextMenuItemWithActivityInfo() {
        // TODO(ctzsm): Consider a better way to distinguish app context and |mContext|.
        ContextUtils.initApplicationContextForTests(mContext);
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);

        // test activityInfo exported=false
        List<ResolveInfo> list1 = new ArrayList();
        ResolveInfo resolveInfo1 = createResolveInfoWithActivityInfo("ProcessTextActivity1", false);
        list1.add(resolveInfo1);
        when(mPackageManager.queryIntentActivities(any(Intent.class), anyInt())).thenReturn(list1);

        Menu menu1 = new RoboMenu();
        assertEquals(0, menu1.size());
        spyController.initializeTextProcessingMenuForTesting(mActionMode, menu1);
        assertEquals(0, menu1.size());

        // test activityInfo exported=true
        List<ResolveInfo> list2 = new ArrayList();
        ResolveInfo resolveInfo2 = createResolveInfoWithActivityInfo("ProcessTextActivity2", true);
        list2.add(resolveInfo2);
        when(mPackageManager.queryIntentActivities(any(Intent.class), anyInt())).thenReturn(list2);

        Menu menu2 = new RoboMenu();
        assertEquals(0, menu2.size());
        spyController.initializeTextProcessingMenuForTesting(mActionMode, menu2);
        assertEquals(1, menu2.size());

        // test null activityInfo
        List<ResolveInfo> list3 = new ArrayList();
        ResolveInfo resolveInfo3 = new ResolveInfo();
        resolveInfo3.activityInfo = null;
        list3.add(resolveInfo3);
        when(mPackageManager.queryIntentActivities(any(Intent.class), anyInt())).thenReturn(list3);

        Menu menu3 = new RoboMenu();
        assertEquals(0, menu3.size());
        spyController.initializeTextProcessingMenuForTesting(mActionMode, menu3);
        assertEquals(0, menu3.size());
    }

    @Test
    @Feature({"TextInput"})
    public void testShowDropdownMenuWhenMenuSourceIsMouse() {
        setDropdownMenuFeatureEnabled(true);
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        SelectionDropdownMenuDelegate dropdownMenuDelegate =
                Mockito.spy(new TestSelectionDropdownMenuDelegate());
        spyController.setDropdownMenuDelegate(dropdownMenuDelegate);
        showSelectionMenu(
                spyController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);
        Mockito.verify(spyController, times(1)).createAndShowDropdownMenu();
        Mockito.verify(spyController, times(1)).destroyActionModeAndKeepSelection();
        Mockito.verify(dropdownMenuDelegate, times(1)).dismiss();
        Mockito.verify(dropdownMenuDelegate, times(1))
                .show(any(), any(), any(), any(), any(), anyInt(), anyInt());
        Mockito.verify(spyController, never()).showActionModeOrClearOnFailure();
    }

    @Test
    @Feature({"TextInput"})
    public void allItemsArePresentInDropdownMenu() {
        // Adds numItemsPerGroup to each menu group and then builds a dropdown menu. Asserts that
        // all the items were added properly by adding the items to a set at creation time and
        // removing them again when building the dropdown menu.
        int numItemsPerGroup = 8;
        HashSet<Integer> ids = new HashSet<>();
        PendingSelectionMenu pendingMenu = new PendingSelectionMenu(mContext);
        for (@ItemGroupOffset
        int group :
                new int[] {
                    ItemGroupOffset.ASSIST_ITEMS,
                    ItemGroupOffset.DEFAULT_ITEMS,
                    ItemGroupOffset.SECONDARY_ASSIST_ITEMS,
                    ItemGroupOffset.TEXT_PROCESSING_ITEMS
                }) {
            for (int i = 0; i < numItemsPerGroup; i++) {
                int id = group + i;
                pendingMenu.addMenuItem(
                        new SelectionMenuItem.Builder("")
                                .setId(id)
                                .setOrderAndCategory(i, group)
                                .build());
                ids.add(id);
            }
        }
        TestSelectionDropdownMenuDelegate delegate =
                new TestSelectionDropdownMenuDelegate() {
                    @Override
                    public MVCListAdapter.ListItem getMenuItem(
                            String title,
                            @Nullable String contentDescription,
                            int groupId,
                            int id,
                            @Nullable Drawable startIcon,
                            boolean isIconTintable,
                            boolean groupContainsIcon,
                            boolean enabled,
                            @Nullable Intent intent,
                            int order) {
                        ids.remove(id);
                        return super.getMenuItem(
                                title,
                                contentDescription,
                                groupId,
                                id,
                                startIcon,
                                isIconTintable,
                                groupContainsIcon,
                                enabled,
                                intent,
                                order);
                    }
                };
        pendingMenu.getMenuAsDropdown(delegate);
        assertTrue(ids.isEmpty());
    }

    @Test
    @Feature({"TextInput"})
    public void testShowPasteMenuWhenSourceIsLongPressWithNoSelection() {
        setDropdownMenuFeatureEnabled(true);
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        SelectionDropdownMenuDelegate dropdownMenuDelegate =
                Mockito.spy(new TestSelectionDropdownMenuDelegate());
        spyController.setDropdownMenuDelegate(dropdownMenuDelegate);
        showSelectionMenu(
                spyController, "", /* selectionStartOffset= */ 0, MenuSourceType.LONG_PRESS);
        Mockito.verify(spyController, times(1)).showActionModeOrClearOnFailure();
        Mockito.verify(dropdownMenuDelegate, times(1)).dismiss();
        Mockito.verify(spyController, never()).createAndShowDropdownMenu();
    }

    @Test
    @Feature({"TextInput"})
    public void testShowSelectionActionMenuWhenSourceIsLongPressWithSelection() {
        setDropdownMenuFeatureEnabled(true);
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        SelectionDropdownMenuDelegate dropdownMenuDelegate =
                Mockito.spy(new TestSelectionDropdownMenuDelegate());
        spyController.setDropdownMenuDelegate(dropdownMenuDelegate);
        showSelectionMenu(
                spyController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.LONG_PRESS);
        Mockito.verify(spyController, times(1)).showActionModeOrClearOnFailure();
        Mockito.verify(dropdownMenuDelegate, times(1)).dismiss();
        Mockito.verify(spyController, never()).createAndShowDropdownMenu();
    }

    @Test
    public void testMenuIsCachedForSameSelectionStateIfDelegateIsNull() {
        Assert.assertNull(mController.getSelectionMenuCachedResultForTesting());

        // Called twice to check the selection menu has been cached properly.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        PendingSelectionMenu pendingMenu = mController.getPendingSelectionMenu(MenuType.FLOATING);
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        Assert.assertNotNull(mController.getSelectionMenuCachedResultForTesting());
        Assert.assertSame(pendingMenu, mController.getPendingSelectionMenu(MenuType.FLOATING));
    }

    @Test
    public void testMenuIsProcessedForSameSelectionStateIfCachingNotEnabledByDelegate() {
        Assert.assertNull(mController.getSelectionMenuCachedResultForTesting());
        SelectionActionMenuDelegate delegate = Mockito.mock(SelectionActionMenuDelegate.class);
        mController.setSelectionActionMenuDelegate(delegate);
        when(delegate.canReuseCachedSelectionMenu(anyInt())).thenReturn(false);
        when(delegate.getDefaultMenuItemOrder(anyInt()))
                .thenReturn(SelectionActionMenuDelegate.getDefaultMenuItemOrder());

        // Called twice to check the selection menu has been cached properly.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        PendingSelectionMenu pendingMenu = mController.getPendingSelectionMenu(MenuType.FLOATING);
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        Assert.assertNotNull(mController.getSelectionMenuCachedResultForTesting());
        Assert.assertNotSame(pendingMenu, mController.getPendingSelectionMenu(MenuType.FLOATING));
    }

    @Test
    public void testMenuIsCachedForSameSelectionStateIfCachingEnabledByDelegate() {
        Assert.assertNull(mController.getSelectionMenuCachedResultForTesting());
        SelectionActionMenuDelegate delegate = Mockito.mock(SelectionActionMenuDelegate.class);
        mController.setSelectionActionMenuDelegate(delegate);
        when(delegate.canReuseCachedSelectionMenu(anyInt())).thenReturn(true);
        when(delegate.getDefaultMenuItemOrder(anyInt()))
                .thenReturn(SelectionActionMenuDelegate.getDefaultMenuItemOrder());

        // Called twice to check the selection menu has been cached properly.
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        PendingSelectionMenu pendingMenu = mController.getPendingSelectionMenu(MenuType.FLOATING);
        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        Assert.assertNotNull(mController.getSelectionMenuCachedResultForTesting());
        Assert.assertSame(pendingMenu, mController.getPendingSelectionMenu(MenuType.FLOATING));
    }

    @Test
    public void testNewMenuIsProcessedForDifferentSelectionState() {
        Assert.assertNull(mController.getSelectionMenuCachedResultForTesting());

        showSelectionMenu(
                mController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);

        PendingSelectionMenu pendingMenu = mController.getPendingSelectionMenu(MenuType.FLOATING);
        showSelectionMenu(
                mController, AMPHITHEATRE, /* selectionStartOffset= */ 0, MenuSourceType.MOUSE);

        // Check the menu is different and not similar to the one we have stored.
        Assert.assertNotNull(mController.getSelectionMenuCachedResultForTesting());
        Assert.assertNotSame(pendingMenu, mController.getPendingSelectionMenu(MenuType.FLOATING));
        Assert.assertNotSame(
                mController.getSelectionMenuCachedResultForTesting(),
                mController.getPendingSelectionMenu(MenuType.FLOATING));
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnSelectionHandlesShownEvent() {
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_SHOWN, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnSelectionHandlesMovedEvent() {
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_MOVED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testSetExclusionRectsOnSelectionHandlesClearedEvent() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", 29);
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLES_CLEARED, 0, 0, 0, 0);
        Mockito.verify(mView, times(1))
                .setSystemGestureExclusionRects(List.of(new Rect(0, 0, 0, 0)));
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnSelectionHandlesDragStartedEvent() {
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLE_DRAG_STARTED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnInsertionHandlesShownEvent() {
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_SHOWN, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnInsertionHandlesMovedEvent() {
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_MOVED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnInsertionHandleTappedEvent() {
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_TAPPED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnInsertionHandleClearedEvent() {
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_CLEARED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnInsertionHandleDragStartedEvent() {
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_DRAG_STARTED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsOnInsertionHandleDragStoppedEvent() {
        mController.onSelectionEvent(SelectionEventType.INSERTION_HANDLE_DRAG_STOPPED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature({"TextInput"})
    public void testSetExclusionRectsOnSelectionHandleDragStopped() {
        SelectionPopupControllerImpl mockController =
                Mockito.spy(
                        SelectionPopupControllerImpl.createForTesting(
                                mWebContents, mPopupController));
        when(mWebContents.getRenderCoordinates()).thenReturn(mRenderCoordinates);
        when(mRenderCoordinates.getDeviceScaleFactor()).thenReturn(1.0f);
        when(mRenderCoordinates.getContentOffsetYPix()).thenReturn(0.0f);
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", 29);
        Object[] handleRects = new Rect[2];
        handleRects[0] = new Rect(0, 0, 10, 10);
        handleRects[1] = new Rect(10, 10, 20, 20);
        List<Rect> rects = new ArrayList<>();
        rects.add((Rect) handleRects[0]);
        rects.add((Rect) handleRects[1]);
        when(mockController.getTouchHandleRects()).thenReturn(handleRects);
        mockController.onSelectionEvent(
                SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 0, 0, 0, 0);
        Mockito.verify(mView, times(1)).setSystemGestureExclusionRects(rects);
    }

    @Test
    @Feature({"TextInput"})
    public void testNotSetExclusionRectsWithNullView() {
        ReflectionHelpers.setStaticField(Build.VERSION.class, "SDK_INT", 29);
        when(mWebContents.getViewAndroidDelegate()).thenReturn(null);
        mController.onSelectionEvent(SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED, 0, 0, 0, 0);
        Mockito.verify(mView, never()).setSystemGestureExclusionRects(anyList());
    }

    @Test
    @Feature("ExtensionContextMenuItems")
    public void testExtraMenuItems() {
        setDropdownMenuFeatureEnabled(true);
        List<ListItem> extraItems =
                List.of(
                        new ListItem(
                                MENU_ITEM,
                                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                                        .with(TITLE, EXTRA_MENU_ITEM_TITLE)
                                        .build()));
        when(mMenuModelBridge.getListItems()).thenReturn(extraItems);
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        SelectionDropdownMenuDelegate dropdownMenuDelegate =
                Mockito.spy(new TestSelectionDropdownMenuDelegate());
        spyController.setDropdownMenuDelegate(dropdownMenuDelegate);
        showSelectionMenu(
                spyController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);
        ArgumentCaptor<ModelList> modelList = ArgumentCaptor.captor();
        Mockito.verify(dropdownMenuDelegate, times(1))
                .show(any(), any(), modelList.capture(), any(), any(), anyInt(), anyInt());
        // Assert that extra item inserted at end.
        ModelList result = modelList.getValue();
        ListItem lastItem = result.get(result.size() - 1);
        assertEquals(
                "Expected extra item to have title " + EXTRA_MENU_ITEM_TITLE,
                EXTRA_MENU_ITEM_TITLE,
                lastItem.model.get(TITLE));
    }

    @Test
    @Feature("ExtensionContextMenuItems")
    public void testSubMenuInDropdownMenu() {
        setDropdownMenuFeatureEnabled(true);

        // Create submenu items
        List<ListItem> submenuItems = new ArrayList<>();
        PropertyModel submenuItem1 =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Submenu Item 1")
                        .build();
        submenuItems.add(new ListItem(MENU_ITEM, submenuItem1));

        // Create main menu item with submenu
        PropertyModel mainMenuItem =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, "Main Menu Item with Submenu")
                        .with(ListMenuSubmenuItemProperties.SUBMENU_ITEMS, submenuItems)
                        .build();
        ListItem mainListItem = new ListItem(MENU_ITEM, mainMenuItem);

        List<ListItem> items = List.of(mainListItem);

        when(mMenuModelBridge.getListItems()).thenReturn(items);
        SelectionPopupControllerImpl spyController = Mockito.spy(mController);
        spyController.setActionModeCallback(mActionModeCallback);
        SelectionDropdownMenuDelegate dropdownMenuDelegate =
                Mockito.spy(new TestSelectionDropdownMenuDelegate());
        spyController.setDropdownMenuDelegate(dropdownMenuDelegate);
        showSelectionMenu(
                spyController,
                AMPHITHEATRE_FULL,
                /* selectionStartOffset= */ 0,
                MenuSourceType.MOUSE);
        ArgumentCaptor<SelectionDropdownMenuDelegate.ItemClickListener> clickListenerCaptor =
                ArgumentCaptor.forClass(SelectionDropdownMenuDelegate.ItemClickListener.class);
        Mockito.verify(dropdownMenuDelegate, times(1))
                .show(
                        any(),
                        any(),
                        any(),
                        clickListenerCaptor.capture(),
                        any(),
                        anyInt(),
                        anyInt());
        SelectionDropdownMenuDelegate.ItemClickListener listener = clickListenerCaptor.getValue();

        // Click on the main menu item with submenu, menu should not be dismissed.
        listener.onItemClick(mainListItem.model);
        Mockito.verify(mActionModeCallback, times(1)).onDropdownItemClicked(any(), eq(false));

        // Click on the submenu item, menu should be dismissed.
        listener.onItemClick(submenuItems.get(0).model);
        Mockito.verify(mActionModeCallback, times(1)).onDropdownItemClicked(any(), eq(true));
    }

    private void showSelectionMenu(
            SelectionPopupControllerImpl selectionPopupController,
            String selectedText,
            int selectionStartOffset,
            int sourceType) {
        selectionPopupController.showSelectionMenu(
                0,
                0,
                0,
                0,
                0,
                0,
                0,
                /* isEditable= */ true,
                /* isPasswordType= */ false,
                selectedText,
                selectionStartOffset,
                /* canSelectAll= */ true,
                /* canRichlyEdit= */ true,
                /* shouldSuggest= */ true,
                sourceType,
                mRenderFrameHost,
                mMenuModelBridge);
    }

    private void setDropdownMenuFeatureEnabled(boolean enabled) {
        SelectionPopupControllerImpl.setEnableTabletUiModeForTesting(enabled);
    }

    private ResolveInfo createResolveInfoWithActivityInfo(String activityName, boolean exported) {
        String packageName = "org.chromium.content.browser.selection.SelectionPopupControllerTest";

        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        activityInfo.name = activityName;
        activityInfo.exported = exported;
        activityInfo.applicationInfo = new ApplicationInfo();
        activityInfo.applicationInfo.flags = ApplicationInfo.FLAG_SYSTEM;

        ResolveInfo resolveInfo =
                new ResolveInfo() {
                    @Override
                    public CharSequence loadLabel(PackageManager pm) {
                        return "TEST_LABEL";
                    }
                };
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    // Result generated by long press "Amphitheatre" in "1600 Amphitheatre Parkway".
    private SelectionClient.Result resultForAmphitheatre() {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -5;
        result.endAdjust = 8;
        result.textClassification = createSingleActionTextClassification("Maps");
        return result;
    }

    // Result generated by long press "Mountain" in "585 Franklin Street, Mountain View, CA 94041".
    private SelectionClient.Result resultForMountain() {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = -21;
        result.endAdjust = 15;
        result.textClassification = createSingleActionTextClassification("Maps");
        return result;
    }

    private SelectionClient.Result resultForNoChange() {
        SelectionClient.Result result = new SelectionClient.Result();
        result.startAdjust = 0;
        result.endAdjust = 0;
        result.textClassification = createSingleActionTextClassification("Maps");
        return result;
    }

    private TextClassification createSingleActionTextClassification(String title) {
        Icon actionIcon = Icon.createWithData(new byte[] {}, 0, 0);
        PendingIntent intent =
                PendingIntent.getBroadcast(
                        mContext,
                        0,
                        new Intent(),
                        IntentUtils.getPendingIntentMutabilityFlag(false));
        RemoteAction action = new RemoteAction(actionIcon, title, "This is a menu item", intent);
        return new TextClassification.Builder().addAction(action).build();
    }
}
