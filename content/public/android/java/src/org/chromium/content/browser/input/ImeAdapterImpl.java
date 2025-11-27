// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.os.SystemClock;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.BackgroundColorSpan;
import android.text.style.CharacterStyle;
import android.text.style.ForegroundColorSpan;
import android.text.style.SuggestionSpan;
import android.text.style.UnderlineSpan;
import android.util.SparseArray;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.DeleteGesture;
import android.view.inputmethod.DeleteRangeGesture;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.ExtractedText;
import android.view.inputmethod.HandwritingGesture;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.view.inputmethod.InsertGesture;
import android.view.inputmethod.JoinOrSplitGesture;
import android.view.inputmethod.RemoveSpaceGesture;
import android.view.inputmethod.SelectGesture;
import android.view.inputmethod.SelectRangeGesture;

import androidx.annotation.VisibleForTesting;
import androidx.core.view.inputmethod.EditorInfoCompat;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.UserData;
import org.chromium.blink.mojom.EventType;
import org.chromium.blink.mojom.FocusType;
import org.chromium.blink.mojom.HandwritingGestureResult;
import org.chromium.blink.mojom.InputCursorAnchorInfo;
import org.chromium.blink.mojom.StylusWritingGestureData;
import org.chromium.blink_public.web.WebInputEventModifier;
import org.chromium.blink_public.web.WebTextInputMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.picker.InputDialogContainer;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.InputMethodManagerWrapper;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContents.UserDataFactory;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.base.ime.TextInputAction;
import org.chromium.ui.base.ime.TextInputType;
import org.chromium.ui.mojom.ImeTextSpanType;
import org.chromium.ui.mojom.VirtualKeyboardPolicy;
import org.chromium.ui.mojom.VirtualKeyboardVisibilityRequest;

import java.lang.ref.WeakReference;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;

/**
 * Implementation of the interface {@link ImeAdapter} providing an interface in both ways native <->
 * java:
 *
 * <p>1. InputConnectionAdapter notifies native code of text composition state and dispatch key
 * events from java -> WebKit. 2. Native ImeAdapter notifies java side to clear composition text.
 *
 * <p>The basic flow is: 1. When InputConnectionAdapter gets called with composition or result text:
 * If we receive a composition text or a result text, then we just need to dispatch a synthetic key
 * event with special keycode 229, and then dispatch the composition or result text. 2. Intercept
 * dispatchKeyEvent() method for key events not handled by IME, we need to dispatch them to webkit
 * and check webkit's reply. Then inject a new key event for further processing if webkit didn't
 * handle it.
 *
 * <p>Note that the native peer object does not take any strong reference onto the instance of this
 * java object, hence it is up to the client of this class (e.g. the ViewEmbedder implementor) to
 * hold a strong reference to it for the required lifetime of the object.
 */
@JNINamespace("content")
@NullMarked
public class ImeAdapterImpl
        implements ImeAdapter, WindowEventObserver, UserData, InputMethodManagerWrapper.Delegate {
    private static final String TAG = "Ime";
    private static final boolean DEBUG_LOGS = false;

    private static final float SUGGESTION_HIGHLIGHT_BACKGROUND_TRANSPARENCY = 0.4f;

    public static final int COMPOSITION_KEY_CODE = ImeAdapter.COMPOSITION_KEY_CODE;

    // Color used by AOSP Android for a SuggestionSpan with FLAG_EASY_CORRECT set
    private static final int DEFAULT_SUGGESTION_SPAN_COLOR = 0x88C8C8C8;

    private long mNativeImeAdapterAndroid;
    private InputMethodManagerWrapper mInputMethodManagerWrapper;
    private @Nullable ChromiumBaseInputConnection mInputConnection;
    private ChromiumBaseInputConnection.@Nullable Factory mInputConnectionFactory;

    // NOTE: This object will not be released by Android framework until the matching
    // ResultReceiver in the InputMethodService (IME app) gets gc'ed.
    private @Nullable ShowKeyboardResultReceiver mShowKeyboardResultReceiver;

    private final WebContentsImpl mWebContents;
    private final ViewAndroidDelegate mViewDelegate;

    // This holds the information necessary for constructing CursorAnchorInfo, and notifies to
    // InputMethodManager on appropriate timing, depending on how IME requested the information
    // via InputConnection. The update request is per InputConnection, hence for each time it is
    // re-created, the monitoring status will be reset.
    private final CursorAnchorInfoController mCursorAnchorInfoController;

    private final List<ImeEventObserver> mEventObservers = new ArrayList<>();

    private int mTextInputType = TextInputType.NONE;
    private int mTextInputFlags;
    private int mTextInputMode = WebTextInputMode.DEFAULT;
    private int mTextInputAction = TextInputAction.DEFAULT;
    private boolean mNodeEditable;
    private boolean mNodePassword;

    // Viewport rect before the OSK was brought up.
    // Used to tell View#onSizeChanged to focus a form element.
    private final Rect mFocusPreOSKViewportRect = new Rect();

    // Keep the current configuration to detect the change when onConfigurationChanged() is called.
    private Configuration mCurrentConfig;

    private int mLastSelectionStart;
    private int mLastSelectionEnd;

    private String mLastText = "";

    private int mLastCompositionStart;
    private int mLastCompositionEnd;
    private boolean mRestartInputOnNextStateUpdate;
    // Do not access directly, use getStylusWritingImeCallback() instead.
    private @Nullable StylusWritingImeCallback mStylusWritingImeCallback;
    private final SparseArray<OngoingGesture> mOngoingGestures = new SparseArray<>();

    // True if ImeAdapter is connected to render process.
    private boolean mIsConnected;

    // Whether to force show keyboard during stylus handwriting. We do not show it when writing
    // system is active and stylus is used to edit input text. This is used to show the soft
    // keyboard from Direct writing toolbar.
    private boolean mForceShowKeyboardDuringStylusWriting;

    private final ArrayDeque<KeyEvent> mKeyDownEvents = new ArrayDeque<>();

    private String[] mSupportedMimeTypes = {};

    /**
     * {@ResultReceiver} passed in InputMethodManager#showSoftInput}. We need this to scroll to the
     * editable node at the right timing, which is after input method window shows up.
     */
    private static class ShowKeyboardResultReceiver extends ResultReceiver {
        // Unfortunately, the memory life cycle of ResultReceiver object, once passed in
        // showSoftInput(), is in the control of Android's input method framework and IME app,
        // so we use a weakref to avoid tying ImeAdapter's lifetime to that of ResultReceiver
        // object.
        private final WeakReference<ImeAdapterImpl> mImeAdapter;

        public ShowKeyboardResultReceiver(ImeAdapterImpl imeAdapter, Handler handler) {
            super(handler);
            mImeAdapter = new WeakReference<>(imeAdapter);
        }

        @Override
        public void onReceiveResult(int resultCode, Bundle resultData) {
            ImeAdapterImpl imeAdapter = mImeAdapter.get();
            if (imeAdapter == null) return;
            imeAdapter.onShowKeyboardReceiveResult(resultCode);
        }
    }

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<ImeAdapterImpl> INSTANCE = ImeAdapterImpl::new;
    }

    private static final class ImeRenderWidgetHostImpl
            implements org.chromium.blink.mojom.ImeRenderWidgetHost {
        private final WeakReference<ImeAdapterImpl> mImeAdapter;
        private final MessagePipeHandle mHandle;

        ImeRenderWidgetHostImpl(ImeAdapterImpl imeAdapter, MessagePipeHandle handle) {
            mImeAdapter = new WeakReference<>(imeAdapter);
            mHandle = handle;
            org.chromium.blink.mojom.ImeRenderWidgetHost.MANAGER.bind(this, mHandle);
        }

        @Override
        public void updateCursorAnchorInfo(InputCursorAnchorInfo cursorAnchorInfo) {
            ImeAdapterImpl imeAdapter = mImeAdapter.get();
            if (imeAdapter != null) {
                imeAdapter.updateCursorAnchorInfo(cursorAnchorInfo);
            }
        }

        @Override
        public void onConnectionError(MojoException e) {}

        @Override
        public void close() {}
    }

    /**
     * Get {@link ImeAdapter} object used for the give WebContents. {@link #create()} should precede
     * any calls to this.
     *
     * @param webContents {@link WebContents} object.
     * @return {@link ImeAdapter} object.
     */
    public static ImeAdapterImpl fromWebContents(WebContents webContents) {
        ImeAdapterImpl ret =
                webContents.getOrSetUserData(
                        ImeAdapterImpl.class, UserDataFactoryLazyHolder.INSTANCE);
        assert ret != null;
        return ret;
    }

    /** Returns an instance of the default {@link InputMethodManagerWrapper} */
    public static InputMethodManagerWrapper createDefaultInputMethodManagerWrapper(
            Context context,
            @Nullable WindowAndroid windowAndroid,
            InputMethodManagerWrapper.@Nullable Delegate delegate) {
        return new InputMethodManagerWrapperImpl(context, windowAndroid, delegate);
    }

    /**
     * Create {@link ImeAdapterImpl} instance.
     * @param webContents WebContents instance.
     */
    public ImeAdapterImpl(WebContents webContents) {
        mWebContents = (WebContentsImpl) webContents;
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        assert viewDelegate != null;
        mViewDelegate = viewDelegate;

        // Use application context here to avoid leaking the activity context.
        InputMethodManagerWrapper wrapper =
                createDefaultInputMethodManagerWrapper(
                        ContextUtils.getApplicationContext(),
                        mWebContents.getTopLevelNativeWindow(),
                        this);

        // Deep copy newConfig so that we can notice the difference.
        mCurrentConfig = new Configuration(getContainerView().getResources().getConfiguration());

        mCursorAnchorInfoController =
                CursorAnchorInfoController.create(
                        wrapper,
                        new CursorAnchorInfoController.ComposingTextDelegate() {
                            @Override
                            public @Nullable CharSequence getText() {
                                return mLastText;
                            }

                            @Override
                            public int getSelectionStart() {
                                return mLastSelectionStart;
                            }

                            @Override
                            public int getSelectionEnd() {
                                return mLastSelectionEnd;
                            }

                            @Override
                            public int getComposingTextStart() {
                                return mLastCompositionStart;
                            }

                            @Override
                            public int getComposingTextEnd() {
                                return mLastCompositionEnd;
                            }
                        });
        mInputMethodManagerWrapper = wrapper;
        mNativeImeAdapterAndroid = ImeAdapterImplJni.get().init(ImeAdapterImpl.this, mWebContents);
        WindowEventObserverManager.from(mWebContents).addObserver(this);
    }

    @Override
    public @Nullable InputConnection getActiveInputConnection() {
        return mInputConnection;
    }

    @Override
    public @Nullable InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        boolean allowKeyboardLearning = mWebContents != null && !mWebContents.isIncognito();
        InputConnection inputConnection = onCreateInputConnection(outAttrs, allowKeyboardLearning);

        if (mWebContents.getStylusWritingHandler() != null) {
            mWebContents.getStylusWritingHandler().updateEditorInfo(outAttrs);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
            List<Class<? extends HandwritingGesture>> supportedGestures =
                    Arrays.asList(
                            SelectGesture.class,
                            InsertGesture.class,
                            DeleteGesture.class,
                            RemoveSpaceGesture.class,
                            JoinOrSplitGesture.class,
                            SelectRangeGesture.class,
                            DeleteRangeGesture.class);
            outAttrs.setSupportedHandwritingGestures(supportedGestures);
        }
        // Update whether stylus handwriting should be enabled in editor info.
        // This prevents the stylus handwriting toolbar from appearing and ensures the
        // keyboard appears normally on views that do not support stylus handwriting.
        // The null check for StylusWritingHandler indicates whether the feature is
        // enabled and supported for this device.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && mWebContents.getStylusWritingHandler() != null) {
            outAttrs.setStylusHandwritingEnabled(true);
        }
        return inputConnection;
    }

    void handleGesture(OngoingGesture request) {
        mOngoingGestures.put(request.getId(), request);

        // Offset the gesture rectangles to convert from screen coordinates to window coordinates.
        int[] screenLocation = new int[2];
        getContainerView().getLocationOnScreen(screenLocation);
        StylusWritingGestureData gestureData = request.getGestureData();
        gestureData.startRect.x -= screenLocation[0];
        gestureData.startRect.y -= screenLocation[1];
        if (gestureData.endRect != null) {
            gestureData.endRect.x -= screenLocation[0];
            gestureData.endRect.y -= screenLocation[1];
        }

        getStylusWritingImeCallback()
                .handleStylusWritingGestureAction(request.getId(), gestureData);
    }

    @CalledByNative
    private void onStylusWritingGestureActionCompleted(
            int id, @HandwritingGestureResult.EnumType int result) {
        OngoingGesture gesture = mOngoingGestures.get(id);
        if (gesture != null) {
            gesture.onGestureHandled(result);
            mOngoingGestures.remove(id);
        } else {
            assert id == -1;
        }
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return isTextInputType(mTextInputType);
    }

    @Override
    public void onKeyPreIme(int keyCode, KeyEvent event) {
        // HACK: Remember key down events to use it later in sendCompositionToNative().
        // TODO(b/432367402): Use a new Android API to replace this hack with a proper solution.
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_CAPTURE_KEY_EVENTS)
                && Build.VERSION.SDK_INT <= 38) {
            int unicodeChar = event.getUnicodeChar();
            int action = event.getAction();
            if (action == KeyEvent.ACTION_DOWN
                    && unicodeChar != 0
                    && (unicodeChar & KeyCharacterMap.COMBINING_ACCENT) == 0) {
                removeOldKeyDownEvents();
                mKeyDownEvents.add(new KeyEvent(event));
                long maxQueueSize = 1000;
                if (mKeyDownEvents.size() > maxQueueSize) {
                    mKeyDownEvents.remove();
                }
            }
        }
    }

    private void removeOldKeyDownEvents() {
        // Remove events that happened more than a second ago.
        long timestampMs = SystemClock.uptimeMillis();
        long thresholdMs = 1000;
        while (!mKeyDownEvents.isEmpty()
                && timestampMs - mKeyDownEvents.element().getEventTime() >= thresholdMs) {
            mKeyDownEvents.remove();
        }
    }

    /** Whether the focused node is editable or not. */
    @Override
    public boolean focusedNodeEditable() {
        return mTextInputType != TextInputType.NONE;
    }

    private boolean isHardwareKeyboardAttached() {
        return mCurrentConfig.keyboard != Configuration.KEYBOARD_NOKEYS;
    }

    @Override
    public void addEventObserver(ImeEventObserver eventObserver) {
        mEventObservers.add(eventObserver);
    }

    @Override
    public void removeEventObserver(ImeEventObserver imeEventObserver) {
        mEventObservers.remove(imeEventObserver);
    }

    private void createInputConnectionFactory() {
        if (mInputConnectionFactory != null) return;
        mInputConnectionFactory = new ThreadedInputConnectionFactory(mInputMethodManagerWrapper);
    }

    // Tells if the ImeAdapter in valid state (i.e. not in destroyed state), and is
    // connected to render process. The former check guards against the call via
    // ThreadedInputConnection from Android framework after ImeAdapter.destroy() is called.
    private boolean isValid() {
        return mNativeImeAdapterAndroid != 0 && mIsConnected;
    }

    // Whether the focused node allows the soft keyboard to be displayed. A content editable
    // region is editable but may disallow the soft keyboard from being displayed. Composition
    // should still be allowed with a physical keyboard so mInputConnection will be non-null.
    private boolean focusedNodeAllowsSoftKeyboard() {
        return mTextInputType != TextInputType.NONE && mTextInputMode != WebTextInputMode.NONE;
    }

    private View getContainerView() {
        return assumeNonNull(mViewDelegate.getContainerView());
    }

    /**
     * @see View#onCreateInputConnection(EditorInfo)
     * @param allowKeyboardLearning Whether to allow keyboard (IME) app to do personalized learning.
     */
    public @Nullable ChromiumBaseInputConnection onCreateInputConnection(
            EditorInfo outAttrs, boolean allowKeyboardLearning) {
        // InputMethodService evaluates fullscreen mode even when the new input connection is
        // null. This makes sure IME doesn't enter fullscreen mode or open custom UI.
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_FULLSCREEN | EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ANDROID_MEDIA_INSERTION)) {
            mSupportedMimeTypes =
                    ImeAdapterImplJni.get().getSupportedMimeTypes(mNativeImeAdapterAndroid);
            outAttrs.contentMimeTypes = mSupportedMimeTypes;
        }

        if (!allowKeyboardLearning) {
            outAttrs.imeOptions |= EditorInfoCompat.IME_FLAG_NO_PERSONALIZED_LEARNING;
        }

        // Without this line, some third-party IMEs will try to compose text even when
        // not on an editable node. Even when we return null here, key events can still go
        // through ImeAdapter#dispatchKeyEvent().
        if (!focusedNodeEditable()) {
            setInputConnection(null);
            if (DEBUG_LOGS) Log.i(TAG, "onCreateInputConnection returns null.");
            return null;
        }
        if (mInputConnectionFactory == null) return null;
        View containerView = getContainerView();
        if (DEBUG_LOGS) Log.i(TAG, "Last text: " + mLastText);
        setInputConnection(
                mInputConnectionFactory.initializeAndGet(
                        containerView,
                        this,
                        mTextInputType,
                        mTextInputFlags,
                        mTextInputMode,
                        mTextInputAction,
                        mLastSelectionStart,
                        mLastSelectionEnd,
                        mLastText,
                        outAttrs));
        if (DEBUG_LOGS) Log.i(TAG, "onCreateInputConnection: " + mInputConnection);

        if (mCursorAnchorInfoController != null) {
            mCursorAnchorInfoController.onRequestCursorUpdates(
                    false /* not an immediate request */,
                    false /* disable monitoring */,
                    containerView);
        }
        if (isValid()) {
            ImeAdapterImplJni.get()
                    .requestCursorUpdate(
                            mNativeImeAdapterAndroid,
                            false /* not an immediate request */,
                            false /* disable monitoring */);
        }

        if (mInputConnection != null) mInputMethodManagerWrapper.onInputConnectionCreated();
        return mInputConnection;
    }

    private void setInputConnection(@Nullable ChromiumBaseInputConnection inputConnection) {
        if (mInputConnection == inputConnection) return;
        // The previous input connection might be waiting for state update.
        if (mInputConnection != null) mInputConnection.unblockOnUiThread();
        mInputConnection = inputConnection;
    }

    @Override
    public boolean hasInputConnection() {
        return mInputConnection != null;
    }

    @Override
    public void setInputMethodManagerWrapper(InputMethodManagerWrapper immw) {
        mInputMethodManagerWrapper = immw;
        if (mCursorAnchorInfoController != null) {
            mCursorAnchorInfoController.setInputMethodManagerWrapper(immw);
        }
    }

    @VisibleForTesting
    void setInputConnectionFactory(ChromiumBaseInputConnection.Factory factory) {
        mInputConnectionFactory = factory;
    }

    ChromiumBaseInputConnection.@Nullable Factory getInputConnectionFactoryForTest() {
        return mInputConnectionFactory;
    }

    public void setTriggerDelayedOnCreateInputConnectionForTest(boolean trigger) {
        assumeNonNull(mInputConnectionFactory);
        mInputConnectionFactory.setTriggerDelayedOnCreateInputConnection(trigger);
    }

    /** Get the current input connection for testing purposes. */
    @VisibleForTesting
    @Override
    public @Nullable InputConnection getInputConnectionForTest() {
        return mInputConnection;
    }

    @VisibleForTesting
    @Override
    public void setComposingTextForTest(final CharSequence text, final int newCursorPosition) {
        ChromiumBaseInputConnection inputConnection = assumeNonNull(mInputConnection);
        inputConnection
                .getHandler()
                .post(() -> inputConnection.setComposingText(text, newCursorPosition));
    }

    private static int getModifiers(int metaState) {
        int modifiers = 0;
        if ((metaState & KeyEvent.META_SHIFT_ON) != 0) {
            modifiers |= WebInputEventModifier.SHIFT_KEY;
        }
        if ((metaState & KeyEvent.META_ALT_ON) != 0) {
            modifiers |= WebInputEventModifier.ALT_KEY;
        }
        if ((metaState & KeyEvent.META_CTRL_ON) != 0) {
            modifiers |= WebInputEventModifier.CONTROL_KEY;
        }
        if ((metaState & KeyEvent.META_CAPS_LOCK_ON) != 0) {
            modifiers |= WebInputEventModifier.CAPS_LOCK_ON;
        }
        if ((metaState & KeyEvent.META_NUM_LOCK_ON) != 0) {
            modifiers |= WebInputEventModifier.NUM_LOCK_ON;
        }
        return modifiers;
    }

    private void updateInputStateForStylusWriting() {
        if (mWebContents.getStylusWritingHandler() == null) return;
        mWebContents
                .getStylusWritingHandler()
                .updateInputState(mLastText, mLastSelectionStart, mLastSelectionEnd);
    }

    /** Retrieves the supported MIME types of the current input field. */
    public String[] getSupportedMimeTypes() {
        return mSupportedMimeTypes;
    }

    /**
     * Updates internal representation of the text being edited and its selection and composition
     * properties.
     *
     * @param textInputType Text input type for the currently focused field in renderer.
     * @param textInputFlags Text input flags.
     * @param textInputMode Text input mode.
     * @param textInputAction Text input mode action.
     * @param showIfNeeded Whether the keyboard should be shown if it is currently hidden.
     * @param alwaysHide Whether the keyboard should be unconditionally hidden.
     * @param text The String contents of the field being edited.
     * @param selectionStart The character offset of the selection start, or the caret position if
     *     there is no selection.
     * @param selectionEnd The character offset of the selection end, or the caret position if there
     *     is no selection.
     * @param compositionStart The character offset of the composition start, or -1 if there is no
     *     composition.
     * @param compositionEnd The character offset of the composition end, or -1 if there is no
     *     selection.
     * @param replyToRequest True when the update was requested by IME.
     * @param lastVkVisibilityRequest VK visibility request type if show/hide APIs are called from
     *     JS.
     * @param vkPolicy VK policy type whether it is manual or automatic.
     * @param imeTextSpans an array of span information (such as spelling and grammar markers).
     */
    @VisibleForTesting
    @CalledByNative
    /*package*/ void updateState(
            int textInputType,
            int textInputFlags,
            int textInputMode,
            int textInputAction,
            boolean showIfNeeded,
            boolean alwaysHide,
            String text,
            int selectionStart,
            int selectionEnd,
            int compositionStart,
            int compositionEnd,
            boolean replyToRequest,
            int lastVkVisibilityRequest,
            int vkPolicy,
            ImeTextSpan[] imeTextSpans) {
        TraceEvent.begin("ImeAdapter.updateState");
        try {
            if (DEBUG_LOGS) {
                Log.i(
                        TAG,
                        "updateState: type [%d->%d], flags [%d], mode[%d], action[%d], show [%b],"
                                + " hide [%b]",
                        mTextInputType,
                        textInputType,
                        textInputFlags,
                        textInputMode,
                        textInputAction,
                        showIfNeeded,
                        alwaysHide);
            }
            boolean needsRestart = false;
            boolean hide = false;
            if (mRestartInputOnNextStateUpdate) {
                needsRestart = true;
                mRestartInputOnNextStateUpdate = false;
            }

            mTextInputFlags = textInputFlags;
            if (mTextInputMode != textInputMode) {
                mTextInputMode = textInputMode;
                hide = textInputMode == WebTextInputMode.NONE && !isHardwareKeyboardAttached();
                needsRestart = true;
            }
            if (mTextInputType != textInputType) {
                mTextInputType = textInputType;
                if (textInputType == TextInputType.NONE) {
                    hide = true;
                } else {
                    needsRestart = true;
                }
            } else if (textInputType == TextInputType.NONE) {
                hide = true;
            }
            if (mTextInputAction != textInputAction) {
                mTextInputAction = textInputAction;
                needsRestart = true;
            }

            boolean editable = focusedNodeEditable();
            boolean password = textInputType == TextInputType.PASSWORD;
            updateNodeAttributes(editable, password);
            if (mCursorAnchorInfoController != null
                    && (!TextUtils.equals(mLastText, text)
                            || mLastSelectionStart != selectionStart
                            || mLastSelectionEnd != selectionEnd
                            || mLastCompositionStart != compositionStart
                            || mLastCompositionEnd != compositionEnd)) {
                mCursorAnchorInfoController.invalidateLastCursorAnchorInfo();
            }
            mLastText = text;
            mLastSelectionStart = selectionStart;
            mLastSelectionEnd = selectionEnd;
            mLastCompositionStart = compositionStart;
            mLastCompositionEnd = compositionEnd;

            // Check for the visibility request and policy if VK APIs are enabled.
            if (vkPolicy == VirtualKeyboardPolicy.MANUAL) {
                // policy is manual.
                if (lastVkVisibilityRequest == VirtualKeyboardVisibilityRequest.SHOW) {
                    showSoftKeyboard();
                } else if (lastVkVisibilityRequest == VirtualKeyboardVisibilityRequest.HIDE) {
                    hideKeyboard();
                }
            } else {
                if (hide || alwaysHide) {
                    hideKeyboard();
                } else {
                    if (needsRestart) restartInput();
                    if (showIfNeeded && focusedNodeAllowsSoftKeyboard()) {
                        // There is no API for us to get notified of user's dismissal of keyboard.
                        // Therefore, we should try to show keyboard even when text input type
                        // hasn't changed.
                        showSoftKeyboard();
                    }
                }
            }

            if (mInputConnection != null) {
                boolean singleLine =
                        mTextInputType != TextInputType.TEXT_AREA
                                && mTextInputType != TextInputType.CONTENT_EDITABLE;

                CharSequence textParam;
                if (imeTextSpans == null || imeTextSpans.length == 0) {
                    textParam = text;
                } else {
                    SpannableStringBuilder spannable = new SpannableStringBuilder(text);
                    for (ImeTextSpan info : imeTextSpans) {
                        int flags = 0;
                        if (info.getType() == ImeTextSpanType.MISSPELLING_SUGGESTION) {
                            flags = SuggestionSpan.FLAG_MISSPELLED;
                        } else if (info.getType() == ImeTextSpanType.GRAMMAR_SUGGESTION) {
                            flags = SuggestionSpan.FLAG_GRAMMAR_ERROR;
                        }

                        // When we decide to show the system's suggestion menu, then we should use
                        // FLAG_EASY_CORRECT to tell the IME not to show their custom suggestion
                        // menu.
                        if (!info.shouldHideSuggestionMenu()) {
                            flags = flags | SuggestionSpan.FLAG_EASY_CORRECT;
                        }

                        SuggestionSpan suggestionSpan =
                                new SuggestionSpan(
                                        ContextUtils.getApplicationContext(),
                                        info.getSuggestions(),
                                        flags);

                        spannable.setSpan(
                                suggestionSpan,
                                info.getStartOffset(),
                                info.getEndOffset(),
                                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
                    }
                    textParam = spannable;
                }

                mInputConnection.updateStateOnUiThread(
                        textParam,
                        selectionStart,
                        selectionEnd,
                        compositionStart,
                        compositionEnd,
                        singleLine,
                        replyToRequest);
            }
        } finally {
            TraceEvent.end("ImeAdapter.updateState");
        }

        // Update input state to stylus writing service.
        updateInputStateForStylusWriting();
    }

    /** Show soft keyboard only if it is the current keyboard configuration. */
    private void showSoftKeyboard() {
        if (!isValid()) return;
        if (DEBUG_LOGS) Log.i(TAG, "showSoftKeyboard");
        View containerView = getContainerView();

        // Block showing soft keyboard during stylus handwriting.
        int lastToolType = mWebContents.getEventForwarder().getLastToolType();
        if (mWebContents.getStylusWritingHandler() != null
                && !mWebContents.getStylusWritingHandler().canShowSoftKeyboard()
                && (lastToolType == MotionEvent.TOOL_TYPE_STYLUS
                        || lastToolType == MotionEvent.TOOL_TYPE_ERASER)
                && mTextInputType != TextInputType.PASSWORD
                && !mForceShowKeyboardDuringStylusWriting) {
            if (DEBUG_LOGS) Log.i(TAG, "showSoftKeyboard: blocked during stylus writing");
            return;
        }

        mInputMethodManagerWrapper.showSoftInput(containerView, 0, getNewShowKeyboardReceiver());
        if (containerView.getResources().getConfiguration().keyboard
                != Configuration.KEYBOARD_NOKEYS) {
            mWebContents.scrollFocusedEditableNodeIntoView();
        }
    }

    private void updateNodeAttributes(boolean isEditable, boolean isPassword) {
        if (mNodeEditable != isEditable || mNodePassword != isPassword) {
            for (ImeEventObserver observer : mEventObservers) {
                observer.onNodeAttributeUpdated(isEditable, isPassword);
            }
            mNodeEditable = isEditable;
            mNodePassword = isPassword;
        }
    }

    @Override
    public void onShowKeyboardReceiveResult(int resultCode) {
        if (!isValid()) return;
        View containerView = getContainerView();
        if (resultCode == InputMethodManager.RESULT_SHOWN) {
            // If OSK is newly shown, delay the form focus until
            // the onSizeChanged (in order to adjust relative to the
            // new size).
            // TODO(jdduke): We should not assume that onSizeChanged will
            // always be called, crbug.com/294908.
            containerView.getWindowVisibleDisplayFrame(mFocusPreOSKViewportRect);
        } else if (ViewUtils.hasFocus(containerView)
                && resultCode == InputMethodManager.RESULT_UNCHANGED_SHOWN) {
            // If the OSK was already there, focus the form immediately.
            // Also, the VR soft keyboard always reports RESULT_UNCHANGED_SHOWN as it
            // doesn't affect the size of the web contents.
            mWebContents.scrollFocusedEditableNodeIntoView();
        }
    }

    @CalledByNative
    private void updateOnTouchDown() {
        cancelRequestToScrollFocusedEditableNodeIntoView();
    }

    public void cancelRequestToScrollFocusedEditableNodeIntoView() {
        mFocusPreOSKViewportRect.setEmpty();
    }

    @Override
    public ResultReceiver getNewShowKeyboardReceiver() {
        if (mShowKeyboardResultReceiver == null) {
            // Note: the returned object will get leaked by Android framework.
            mShowKeyboardResultReceiver = new ShowKeyboardResultReceiver(this, new Handler());
        }
        return mShowKeyboardResultReceiver;
    }

    /** Hide soft keyboard. */
    private void hideKeyboard() {
        if (!isValid()) return;
        if (DEBUG_LOGS) Log.i(TAG, "hideKeyboard");
        View view = mViewDelegate.getContainerView();
        if (view != null && mInputMethodManagerWrapper.isActive(view)) {
            // NOTE: we should not set ResultReceiver here. Otherwise, IMM will own
            // ImeAdapter even after input method goes away and result gets received.
            mInputMethodManagerWrapper.hideSoftInputFromWindow(view.getWindowToken(), 0, null);
        }
        // Detach input connection by returning null from onCreateInputConnection().
        if (!focusedNodeEditable() && mInputConnection != null) {
            ChromiumBaseInputConnection inputConnection = mInputConnection;
            restartInput(); // resets mInputConnection
            // crbug.com/666982: Restart input may not happen if view is detached from window, but
            // we need to unblock in any case. We want to call this after restartInput() to
            // ensure that there is no additional IME operation in the queue.
            inputConnection.unblockOnUiThread();
        }
    }

    // WindowEventObserver

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        if (!isValid()) return;
        // If configuration unchanged, do nothing.
        if (mCurrentConfig.keyboard == newConfig.keyboard
                && mCurrentConfig.keyboardHidden == newConfig.keyboardHidden
                && mCurrentConfig.hardKeyboardHidden == newConfig.hardKeyboardHidden) {
            return;
        }

        // Deep copy newConfig so that we can notice the difference.
        mCurrentConfig = new Configuration(newConfig);
        if (DEBUG_LOGS) {
            Log.i(TAG, "onKeyboardConfigurationChanged: mTextInputType [%d]", mTextInputType);
        }
        if (focusedNodeAllowsSoftKeyboard()) {
            restartInput();
            // By default, we show soft keyboard on keyboard changes. This is useful
            // when the user switches from hardware keyboard to software keyboard.
            // TODO(changwan): check if we can skip this for hardware keyboard configurations.
            showSoftKeyboard();
        } else if (focusedNodeEditable()) {
            // The focused node is editable but disllows the virtual keyboard. We may need to
            // show soft keyboard (for IME composition window only) if a hardware keyboard is
            // present.
            restartInput();
            if (!isHardwareKeyboardAttached()) {
                hideKeyboard();
            } else {
                showSoftKeyboard();
            }
        }
    }

    @Override
    public void onWindowFocusChanged(boolean gainFocus) {
        if (mInputConnectionFactory != null) {
            mInputConnectionFactory.onWindowFocusChanged(gainFocus);
        }
    }

    @Override
    public void onWindowAndroidChanged(@Nullable WindowAndroid windowAndroid) {
        if (mInputMethodManagerWrapper != null) {
            mInputMethodManagerWrapper.onWindowAndroidChanged(windowAndroid);
        }
    }

    @Override
    public void onAttachedToWindow() {
        if (mInputConnectionFactory != null) {
            mInputConnectionFactory.onViewAttachedToWindow();
        }
    }

    @Override
    public void onDetachedFromWindow() {
        resetAndHideKeyboard();
        if (mInputConnectionFactory != null) {
            mInputConnectionFactory.onViewDetachedFromWindow();
        }
    }

    @Override
    public void onViewFocusChanged(boolean gainFocus, boolean hideKeyboardOnBlur) {
        if (DEBUG_LOGS) Log.i(TAG, "onViewFocusChanged: gainFocus [%b]", gainFocus);
        if (!gainFocus && hideKeyboardOnBlur) resetAndHideKeyboard();
        if (mInputConnectionFactory != null) {
            mInputConnectionFactory.onViewFocusChanged(gainFocus);
        }
    }

    /** Resets IME adapter and hides keyboard. Note that this will also unblock input connection. */
    @Override
    public void resetAndHideKeyboard() {
        if (DEBUG_LOGS) Log.i(TAG, "resetAndHideKeyboard");
        mTextInputType = TextInputType.NONE;
        mTextInputFlags = 0;
        mTextInputMode = WebTextInputMode.DEFAULT;
        mRestartInputOnNextStateUpdate = false;
        updateNodeAttributes(/* isEditable= */ false, mNodePassword);
        // This will trigger unblocking if necessary.
        hideKeyboard();
    }

    private static boolean isTextInputType(int type) {
        return type != TextInputType.NONE && !InputDialogContainer.isDialogInputType(type);
    }

    /** See {@link View#dispatchKeyEvent(KeyEvent)} */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (DEBUG_LOGS) {
            Log.i(
                    TAG,
                    "dispatchKeyEvent: action [%d], keycode [%d]",
                    event.getAction(),
                    event.getKeyCode());
        }
        if (mInputConnection != null) return mInputConnection.sendKeyEventOnUiThread(event);
        return sendKeyEvent(event);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        resetAndHideKeyboard();
        mNativeImeAdapterAndroid = 0;
        mIsConnected = false;
        if (mCursorAnchorInfoController != null) {
            mCursorAnchorInfoController.focusedNodeChanged(false);
        }
        mStylusWritingImeCallback = null;
        if (mWebContents.getStylusWritingHandler() != null) {
            mWebContents.getStylusWritingHandler().onImeAdapterDestroyed();
        }
    }

    /**
     * Update selection to input method manager.
     *
     * @param selectionStart   The selection start.
     * @param selectionEnd     The selection end.
     * @param compositionStart The composition start.
     * @param compositionEnd   The composition end.
     */
    void updateSelection(
            int selectionStart, int selectionEnd, int compositionStart, int compositionEnd) {
        mInputMethodManagerWrapper.updateSelection(
                getContainerView(), selectionStart, selectionEnd, compositionStart, compositionEnd);
    }

    /** Update extracted text to input method manager. */
    void updateExtractedText(int token, @Nullable ExtractedText extractedText) {
        mInputMethodManagerWrapper.updateExtractedText(getContainerView(), token, extractedText);
    }

    /** Restart input (finish composition and change EditorInfo, such as input type). */
    void restartInput() {
        if (!isValid()) return;
        // This will eventually cause input method manager to call View#onCreateInputConnection().
        mInputMethodManagerWrapper.restartInput(getContainerView());
        if (mInputConnection != null) mInputConnection.onRestartInputOnUiThread();
    }

    /**
     * @see BaseInputConnection#performContextMenuAction(int)
     */
    boolean performContextMenuAction(int id) {
        if (DEBUG_LOGS) Log.i(TAG, "performContextMenuAction: id [%d]", id);
        switch (id) {
            case android.R.id.selectAll:
                mWebContents.selectAll();
                return true;
            case android.R.id.cut:
                mWebContents.cut();
                return true;
            case android.R.id.copy:
                mWebContents.copy();
                return true;
            case android.R.id.paste:
                mWebContents.paste();
                return true;
            default:
                return false;
        }
    }

    public boolean performEditorAction(int actionCode) {
        if (!isValid()) return false;

        // If mTextInputAction has been specified (indicating an enterKeyHint
        // has been specified in the HTML) then we do will send the enter key
        // events. Otherwise we fallback to having the enter key move focus
        // between the elements.
        if (mTextInputAction == TextInputAction.DEFAULT) {
            switch (actionCode) {
                case EditorInfo.IME_ACTION_NEXT:
                    advanceFocusForIME(FocusType.FORWARD);
                    return true;
                case EditorInfo.IME_ACTION_PREVIOUS:
                    advanceFocusForIME(FocusType.BACKWARD);
                    return true;
            }
        }
        sendSyntheticKeyPress(
                KeyEvent.KEYCODE_ENTER,
                KeyEvent.FLAG_SOFT_KEYBOARD
                        | KeyEvent.FLAG_KEEP_TOUCH_MODE
                        | KeyEvent.FLAG_EDITOR_ACTION);
        return true;
    }

    /**
     * @see InputConnection#performPrivateCommand(java.lang.String, android.os.Bundle)
     */
    public void performPrivateCommand(String action, Bundle data) {
        mViewDelegate.performPrivateImeCommand(action, data);
    }

    @Override
    public void advanceFocusForIME(int focusType) {
        if (mNativeImeAdapterAndroid == 0) return;
        ImeAdapterImplJni.get().advanceFocusForIME(mNativeImeAdapterAndroid, focusType);
    }

    public void sendSyntheticKeyPress(int keyCode, int flags) {
        long eventTime = SystemClock.uptimeMillis();
        sendKeyEvent(
                new KeyEvent(
                        eventTime,
                        eventTime,
                        KeyEvent.ACTION_DOWN,
                        keyCode,
                        0,
                        0,
                        KeyCharacterMap.VIRTUAL_KEYBOARD,
                        0,
                        flags));
        sendKeyEvent(
                new KeyEvent(
                        eventTime,
                        eventTime,
                        KeyEvent.ACTION_UP,
                        keyCode,
                        0,
                        0,
                        KeyCharacterMap.VIRTUAL_KEYBOARD,
                        0,
                        flags));
    }

    private void onImeEvent() {
        for (ImeEventObserver observer : mEventObservers) observer.onImeEvent();
        if (mNodeEditable && mWebContents.getRenderWidgetHostView() != null) {
            mWebContents.getRenderWidgetHostView().dismissTextHandles();
        }
    }

    boolean sendCompositionToNative(
            CharSequence text, int newCursorPosition, boolean isCommit, int unicodeFromKeyEvent) {
        if (!isValid()) return false;
        onImeEvent();
        long timestampMs = SystemClock.uptimeMillis();

        // HACK: When the user types text using a physical keyboard, Gboard consumes key down events
        // and commits the typed characters even if there is no conversion happening. This doesn't
        // work well with web apps expecting keypress DOM events. b/416494348
        // Ideally Gboard should be fixed to send the consumed key events back to chrome using the
        // sendKeyEvent() API, but as a workaround here we send the corresponding key down event
        // captured in onKeyPreIme() if any.
        if (isCommit && !mKeyDownEvents.isEmpty() && text.length() == 1) {
            removeOldKeyDownEvents();
            // Look for a key down event that matches with the committed text.
            KeyEvent lastKeyDownEvent = null;
            for (KeyEvent event : mKeyDownEvents) {
                if (Character.toString(event.getUnicodeChar()).contentEquals(text)) {
                    lastKeyDownEvent = event;
                    // If there is a matching event, remove all events before it.
                    Iterator<KeyEvent> it = mKeyDownEvents.iterator();
                    while (it.hasNext()) {
                        KeyEvent currentEvent = it.next();
                        it.remove();
                        if (currentEvent == event) {
                            break;
                        }
                    }
                    break;
                }
            }

            if (lastKeyDownEvent != null) {
                if (DEBUG_LOGS) {
                    Log.i(
                            TAG,
                            "sendCompositionToNative: Found a key down event " + lastKeyDownEvent);
                }
                ImeAdapterImplJni.get()
                        .sendKeyEvent(
                                mNativeImeAdapterAndroid,
                                lastKeyDownEvent,
                                EventType.KEY_DOWN,
                                getModifiers(lastKeyDownEvent.getMetaState()),
                                lastKeyDownEvent.getEventTime(),
                                lastKeyDownEvent.getKeyCode(),
                                lastKeyDownEvent.getScanCode(),
                                false,
                                lastKeyDownEvent.getUnicodeChar());
                return true;
            }
        }

        ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        null,
                        EventType.RAW_KEY_DOWN,
                        0,
                        timestampMs,
                        COMPOSITION_KEY_CODE,
                        0,
                        false,
                        unicodeFromKeyEvent);

        if (isCommit) {
            ImeAdapterImplJni.get()
                    .commitText(
                            mNativeImeAdapterAndroid,
                            ImeAdapterImpl.this,
                            text,
                            text.toString(),
                            newCursorPosition);
        } else {
            ImeAdapterImplJni.get()
                    .setComposingText(
                            mNativeImeAdapterAndroid,
                            ImeAdapterImpl.this,
                            text,
                            text.toString(),
                            newCursorPosition);
        }

        ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        null,
                        EventType.KEY_UP,
                        0,
                        timestampMs,
                        COMPOSITION_KEY_CODE,
                        0,
                        false,
                        unicodeFromKeyEvent);
        return true;
    }

    boolean finishComposingText() {
        if (!isValid()) return false;
        ImeAdapterImplJni.get().finishComposingText(mNativeImeAdapterAndroid);
        return true;
    }

    boolean sendKeyEvent(KeyEvent event) {
        if (!isValid()) return false;

        int action = event.getAction();
        int type;
        if (action == KeyEvent.ACTION_DOWN) {
            type = EventType.KEY_DOWN;
        } else if (action == KeyEvent.ACTION_UP) {
            type = EventType.KEY_UP;
        } else {
            // In theory, KeyEvent.ACTION_MULTIPLE is a valid value, but in practice
            // this seems to have been quietly deprecated and we've never observed
            // a case where it's sent (holding down physical keyboard key also
            // sends ACTION_DOWN), so it's fine to silently drop it.
            return false;
        }

        for (ImeEventObserver observer : mEventObservers) observer.onBeforeSendKeyEvent(event);
        onImeEvent();

        return ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        event,
                        type,
                        getModifiers(event.getMetaState()),
                        event.getEventTime(),
                        event.getKeyCode(),
                        event.getScanCode(),
                        /* isSystemKey= */ false,
                        event.getUnicodeChar());
    }

    /**
     * Send a request to the native counterpart to delete a given range of characters.
     * @param beforeLength Number of characters to extend the selection by before the existing
     *                     selection.
     * @param afterLength Number of characters to extend the selection by after the existing
     *                    selection.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean deleteSurroundingText(int beforeLength, int afterLength) {
        onImeEvent();
        if (!isValid()) return false;
        ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        null,
                        EventType.RAW_KEY_DOWN,
                        0,
                        SystemClock.uptimeMillis(),
                        COMPOSITION_KEY_CODE,
                        0,
                        false,
                        0);
        ImeAdapterImplJni.get()
                .deleteSurroundingText(mNativeImeAdapterAndroid, beforeLength, afterLength);
        ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        null,
                        EventType.KEY_UP,
                        0,
                        SystemClock.uptimeMillis(),
                        COMPOSITION_KEY_CODE,
                        0,
                        false,
                        0);
        return true;
    }

    /**
     * Send a request to the native counterpart to delete a given range of characters.
     * @param beforeLength Number of code points to extend the selection by before the existing
     *                     selection.
     * @param afterLength Number of code points to extend the selection by after the existing
     *                    selection.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean deleteSurroundingTextInCodePoints(int beforeLength, int afterLength) {
        onImeEvent();
        if (!isValid()) return false;
        ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        null,
                        EventType.RAW_KEY_DOWN,
                        0,
                        SystemClock.uptimeMillis(),
                        COMPOSITION_KEY_CODE,
                        0,
                        false,
                        0);
        ImeAdapterImplJni.get()
                .deleteSurroundingTextInCodePoints(
                        mNativeImeAdapterAndroid, beforeLength, afterLength);
        ImeAdapterImplJni.get()
                .sendKeyEvent(
                        mNativeImeAdapterAndroid,
                        null,
                        EventType.KEY_UP,
                        0,
                        SystemClock.uptimeMillis(),
                        COMPOSITION_KEY_CODE,
                        0,
                        false,
                        0);
        return true;
    }

    /**
     * Send a request to the native counterpart to replace a given range of characters with the
     * given text.
     *
     * @param start The character index where the replacement should start. Value is 0 or greater
     * @param end the character index where the replacement should end. Value is 0 or greater
     * @param text the text to replace.
     * @param nextCursorPosition the new cursor position around the text.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean replaceText(int start, int end, CharSequence text, int newCursorPosition) {
        if (!isValid()) return false;

        ImeAdapterImplJni.get().finishComposingText(mNativeImeAdapterAndroid);
        ImeAdapterImplJni.get()
                .replaceText(
                        mNativeImeAdapterAndroid,
                        ImeAdapterImpl.this,
                        start,
                        end,
                        text.toString(),
                        newCursorPosition);
        return true;
    }

    /**
     * Send a request to the native counterpart to set the selection to given range.
     *
     * @param start Selection start index.
     * @param end Selection end index.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean setEditableSelectionOffsets(int start, int end) {
        if (!isValid()) return false;
        ImeAdapterImplJni.get().setEditableSelectionOffsets(mNativeImeAdapterAndroid, start, end);
        return true;
    }

    /**
     * Send a request to the native counterpart to set composing region to given indices.
     * @param start The start of the composition.
     * @param end The end of the composition.
     * @return Whether the native counterpart of ImeAdapter received the call.
     */
    boolean setComposingRegion(int start, int end) {
        if (!isValid()) return false;
        if (start <= end) {
            ImeAdapterImplJni.get().setComposingRegion(mNativeImeAdapterAndroid, start, end);
        } else {
            ImeAdapterImplJni.get().setComposingRegion(mNativeImeAdapterAndroid, end, start);
        }
        return true;
    }

    @CalledByNative
    private void focusedNodeChanged(
            boolean isEditable,
            int nodeLeftDip,
            int nodeTopDip,
            int nodeRightDip,
            int nodeBottomDip) {
        if (DEBUG_LOGS) Log.i(TAG, "focusedNodeChanged: isEditable [%b]", isEditable);

        // Update controller before the connection is restarted.
        if (mCursorAnchorInfoController != null) {
            mCursorAnchorInfoController.focusedNodeChanged(isEditable);
        }

        if (mTextInputType != TextInputType.NONE && mInputConnection != null && isEditable) {
            mRestartInputOnNextStateUpdate = true;
        }

        View containerView = getContainerView();

        // Update edit bounds to stylus writing service.
        if (mWebContents.getStylusWritingHandler() != null) {
            RenderCoordinatesImpl coords = mWebContents.getRenderCoordinates();
            Rect editableNodeBounds = new Rect();
            if (isEditable) {
                editableNodeBounds.set(nodeLeftDip, nodeTopDip, nodeRightDip, nodeBottomDip);
            }
            mWebContents
                    .getStylusWritingHandler()
                    .onFocusedNodeChanged(
                            editableNodeBounds,
                            isEditable,
                            containerView,
                            coords.getDeviceScaleFactor(),
                            coords.getContentOffsetYPixInt());
        }

        // Request view system keeps focused element on screen.
        if (ContentFeatureList.sAccessibilityMagnificationFollowsFocus.isEnabled()) {
            Rect nodePix = fromCssToDevicePix(nodeLeftDip, nodeTopDip, nodeRightDip, nodeBottomDip);
            if (!nodePix.isEmpty()) {
                // TODO(crbug.com/464269649): when Baklava 36.1 support lands in Clank, remove
                // delegate indirection and inline `requestInputFocusOnScreen()` call.
                AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
                if (delegate != null) {
                    delegate.requestInputFocusOnScreen(containerView, nodePix);
                }
                // Do nothing if new 36.1 `requestRectangleOnScreen()` API with request source
                // parameter is unavailable.
            }
        }
    }

    @CalledByNative
    private boolean shouldInitiateStylusWriting() {
        if (mWebContents.getStylusWritingHandler() == null) return false;

        // It is possible that current view is not focused when stylus writing is started just after
        // interaction with some other view like Url bar, or share view. We need to focus it so that
        // current web page also gets focused, allowing us to commit text into web input elements.
        View containerView = getContainerView();
        if (!ViewUtils.hasFocus(containerView)) ViewUtils.requestFocus(containerView);

        updateInputStateForStylusWriting();
        return mWebContents.getStylusWritingHandler().shouldInitiateStylusWriting();
    }

    @CalledByNative
    void onEditElementFocusedForStylusWriting(
            int focusedEditLeft,
            int focusedEditTop,
            int focusedEditRight,
            int focusedEditBottom,
            int caretX,
            int caretY) {
        if (mWebContents.getStylusWritingHandler() == null) {
            return;
        }
        float scaleFactor = mWebContents.getRenderCoordinates().getDeviceScaleFactor();
        RectF focusedEditBounds =
                new RectF(focusedEditLeft, focusedEditTop, focusedEditRight, focusedEditBottom);
        Point cursorPosition = new Point(caretX, caretY);
        if (focusedEditBounds.isEmpty()) return;

        int[] screenLocation = new int[2];
        getContainerView().getLocationOnScreen(screenLocation);
        int contentOffsetY = mWebContents.getRenderCoordinates().getContentOffsetYPixInt();
        cursorPosition.offset(screenLocation[0], screenLocation[1] + contentOffsetY);

        Rect roundedBounds = new Rect();
        focusedEditBounds.round(roundedBounds);
        // Send focused edit bounds and caret center position to Stylus writing service.
        mWebContents
                .getStylusWritingHandler()
                .onEditElementFocusedForStylusWriting(
                        roundedBounds,
                        cursorPosition,
                        scaleFactor,
                        contentOffsetY,
                        getContainerView());
    }

    /** Send a request to the native counterpart to give the latest text input state update. */
    boolean requestTextInputStateUpdate() {
        if (!isValid()) return false;
        // You won't get state update anyways.
        if (mInputConnection == null) return false;
        return ImeAdapterImplJni.get().requestTextInputStateUpdate(mNativeImeAdapterAndroid);
    }

    /** Notified when IME requested Chrome to change the cursor update mode. */
    public boolean onRequestCursorUpdates(int cursorUpdateMode) {
        final boolean immediateRequest =
                (cursorUpdateMode & InputConnection.CURSOR_UPDATE_IMMEDIATE) != 0;
        final boolean monitorRequest =
                (cursorUpdateMode & InputConnection.CURSOR_UPDATE_MONITOR) != 0;

        if (isValid()) {
            ImeAdapterImplJni.get()
                    .requestCursorUpdate(
                            mNativeImeAdapterAndroid, immediateRequest, monitorRequest);
        }
        return mCursorAnchorInfoController.onRequestCursorUpdates(
                immediateRequest, monitorRequest, getContainerView());
    }

    /**
     * Sends rich content into the current focused text field
     *
     * @param inputContentInfo information about the rich content to be inserted
     * @return whether the insertion is successful.
     */
    boolean commitContent(String dataUrl) {
        onImeEvent();
        if (!isValid()) return false;
        return ImeAdapterImplJni.get().insertMediaFromURL(mNativeImeAdapterAndroid, dataUrl);
    }

    /** Lazily creates/returns a StylusWritingImeCallback object. */
    public StylusWritingImeCallback getStylusWritingImeCallback() {
        if (mStylusWritingImeCallback == null) {
            mStylusWritingImeCallback =
                    new StylusWritingImeCallback() {
                        @Override
                        public void setEditableSelectionOffsets(int start, int end) {
                            ImeAdapterImpl.this.setEditableSelectionOffsets(start, end);
                        }

                        @Override
                        public void sendCompositionToNative(
                                CharSequence text, int newCursorPosition, boolean isCommit) {
                            ImeAdapterImpl.this.sendCompositionToNative(
                                    text, newCursorPosition, isCommit, 0);
                        }

                        @Override
                        public void performEditorAction(int actionCode) {
                            ImeAdapterImpl.this.performEditorAction(actionCode);
                        }

                        @Override
                        public void showSoftKeyboard() {
                            mForceShowKeyboardDuringStylusWriting = true;
                            ImeAdapterImpl.this.showSoftKeyboard();
                            mForceShowKeyboardDuringStylusWriting = false;
                        }

                        @Override
                        public void hideKeyboard() {
                            ImeAdapterImpl.this.hideKeyboard();
                        }

                        @Override
                        public View getContainerView() {
                            return ImeAdapterImpl.this.getContainerView();
                        }

                        @Override
                        public void resetGestureDetection() {
                            GestureListenerManagerImpl gestureListenerManager =
                                    GestureListenerManagerImpl.fromWebContents(mWebContents);
                            if (gestureListenerManager != null) {
                                gestureListenerManager.resetGestureDetection();
                            }
                        }

                        @Override
                        public void handleStylusWritingGestureAction(
                                int id, StylusWritingGestureData gestureData) {
                            if (mNativeImeAdapterAndroid == 0) return;
                            int contentOffsetY =
                                    (int)
                                            mWebContents
                                                    .getRenderCoordinates()
                                                    .getContentOffsetYPix();
                            gestureData.startRect.y -= contentOffsetY;
                            if (gestureData.endRect != null) {
                                gestureData.endRect.y -= contentOffsetY;
                            }
                            ImeAdapterImplJni.get()
                                    .handleStylusWritingGestureAction(
                                            mNativeImeAdapterAndroid, id, gestureData.serialize());
                        }

                        @Override
                        public void finishComposingText() {
                            ImeAdapterImpl.this.finishComposingText();
                        }
                    };
        }
        return mStylusWritingImeCallback;
    }

    /**
     * Converts bounds from CSS pixels to device pixels, accounting for page scale, device scale,
     * and content Y offset.
     *
     * @param left left X coordinate in CSS pixels
     * @param top top Y coordinate in CSS pixels
     * @param right right X coordinate in CSS pixels
     * @param bottom bottom Y coordinate in CSS pixels
     * @return {@link Rect} with the device pixel equivalents of the provided coordinates.
     */
    private Rect fromCssToDevicePix(float left, float top, float right, float bottom) {
        RenderCoordinatesImpl coords = mWebContents.getRenderCoordinates();
        final int topOffset = coords.getContentOffsetYPixInt();
        return new Rect(
                (int) coords.fromLocalCssToPix(left),
                ((int) coords.fromLocalCssToPix(top)) + topOffset,
                (int) coords.fromLocalCssToPix(right),
                ((int) coords.fromLocalCssToPix(bottom)) + topOffset);
    }

    /**
     * Update the cached CursorAnchorInfo data. This may or may not trigger an update to the
     * platform.
     *
     * @param cursorAnchorInfo the Blink representation of CursorAnchorInfo. Null attributes imply
     *     that no update is needed.
     */
    void updateCursorAnchorInfo(InputCursorAnchorInfo cursorAnchorInfo) {
        View containerView = getContainerView();
        mCursorAnchorInfoController.updateCursorAnchorInfoData(cursorAnchorInfo, containerView);

        // Request view system keep caret on screen when moved.
        if (cursorAnchorInfo.insertionMarker != null
                && ContentFeatureList.sAccessibilityMagnificationFollowsFocus.isEnabled()) {
            // Convert caret bounds from CSS pixels to device pixels relative to root view.
            var caretCss = cursorAnchorInfo.insertionMarker;
            Rect caretPix =
                    fromCssToDevicePix(
                            caretCss.x,
                            caretCss.y,
                            caretCss.x + caretCss.width,
                            caretCss.y + caretCss.height);

            // TODO(crbug.com/464269649): when Baklava 36.1 support lands in Clank, remove delegate
            // indirection and inline `requestRectangleOnScreen()` call.
            AconfigFlaggedApiDelegate delegate = AconfigFlaggedApiDelegate.getInstance();
            if (delegate != null && delegate.requestTextCursorOnScreen(containerView, caretPix)) {
                // Action is performed in condition.
            } else {
                // Fallback to previous API (where `requestRectangleOnScreen()` calls are assumed
                // to come from text cursor moves).
                containerView.requestRectangleOnScreen(caretPix);
            }
        }
    }

    /**
     * This connects the native mojo receiver to its Java implementation. We don't need to keep a
     * reference to the ImeRenderWidgetHost implementation as Mojo will. The implementation does
     * however have a reference to this so that it can call methods on the ImeAdapter.
     *
     * @param nativeHandle the native Mojo receiver's pipe as a native pointer.
     */
    @CalledByNative
    private void bindImeRenderHost(long nativeHandle) {
        MessagePipeHandle handle =
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle();
        new ImeRenderWidgetHostImpl(this, handle);
    }

    /**
     * Notified when a frame has been produced by the renderer and all the associated metadata.
     *
     * @param scaleFactor device scale factor.
     * @param contentOffsetYPix Y offset below the browser controls.
     * @param hasInsertionMarker Whether the insertion marker is visible or not.
     * @param insertionMarkerHorizontal X coordinates (in view-local DIP pixels) of the insertion
     *     marker if it exists. Will be ignored otherwise.
     * @param insertionMarkerTop Y coordinates (in view-local DIP pixels) of the top of the
     *     insertion marker if it exists. Will be ignored otherwise.
     * @param insertionMarkerBottom Y coordinates (in view-local DIP pixels) of the bottom of the
     *     insertion marker if it exists. Will be ignored otherwise.
     */
    @CalledByNative
    private void updateFrameInfo(
            float scaleFactor,
            float contentOffsetYPix,
            boolean hasInsertionMarker,
            boolean isInsertionMarkerVisible,
            float insertionMarkerHorizontal,
            float insertionMarkerTop,
            float insertionMarkerBottom) {
        mCursorAnchorInfoController.onUpdateFrameInfo(
                scaleFactor,
                contentOffsetYPix,
                hasInsertionMarker,
                isInsertionMarkerVisible,
                insertionMarkerHorizontal,
                insertionMarkerTop,
                insertionMarkerBottom,
                getContainerView());
    }

    @CalledByNative
    private void onResizeScrollableViewport(boolean contentsHeightReduced) {
        if (!contentsHeightReduced) {
            cancelRequestToScrollFocusedEditableNodeIntoView();
            return;
        }

        // Execute a delayed form focus operation because the OSK was brought up earlier.
        if (!mFocusPreOSKViewportRect.isEmpty()) {
            Rect rect = new Rect();
            getContainerView().getWindowVisibleDisplayFrame(rect);
            if (!rect.equals(mFocusPreOSKViewportRect)) {
                // Only assume the OSK triggered the onSizeChanged if width was preserved.
                if (rect.width() == mFocusPreOSKViewportRect.width()) {
                    assert mWebContents != null;
                    mWebContents.scrollFocusedEditableNodeIntoView();
                }
                // Zero the rect to prevent the above operation from issuing the delayed
                // form focus event.
                cancelRequestToScrollFocusedEditableNodeIntoView();
            }
        }
    }

    private int getUnderlineColorForSuggestionSpan(SuggestionSpan suggestionSpan) {
        try {
            Method getUnderlineColorMethod = SuggestionSpan.class.getMethod("getUnderlineColor");
            return (int) getUnderlineColorMethod.invoke(suggestionSpan);
        } catch (IllegalAccessException e) {
            return DEFAULT_SUGGESTION_SPAN_COLOR;
        } catch (InvocationTargetException e) {
            return DEFAULT_SUGGESTION_SPAN_COLOR;
        } catch (NoSuchMethodException e) {
            return DEFAULT_SUGGESTION_SPAN_COLOR;
        }
    }

    @CalledByNative
    private void populateImeTextSpansFromJava(CharSequence text, long imeTextSpans) {
        if (DEBUG_LOGS) {
            Log.i(
                    TAG,
                    "populateImeTextSpansFromJava: text [%s], ime_text_spans [%d]",
                    text,
                    imeTextSpans);
        }
        if (!(text instanceof SpannableString)) return;

        SpannableString spannableString = ((SpannableString) text);
        CharacterStyle[] spans = spannableString.getSpans(0, text.length(), CharacterStyle.class);
        for (CharacterStyle span : spans) {
            final int spanFlags = spannableString.getSpanFlags(span);
            if (span instanceof BackgroundColorSpan) {
                ImeAdapterImplJni.get()
                        .appendBackgroundColorSpan(
                                imeTextSpans,
                                spannableString.getSpanStart(span),
                                spannableString.getSpanEnd(span),
                                ((BackgroundColorSpan) span).getBackgroundColor());
            } else if (span instanceof ForegroundColorSpan) {
                ImeAdapterImplJni.get()
                        .appendForegroundColorSpan(
                                imeTextSpans,
                                spannableString.getSpanStart(span),
                                spannableString.getSpanEnd(span),
                                ((ForegroundColorSpan) span).getForegroundColor());
            } else if (span instanceof UnderlineSpan) {
                ImeAdapterImplJni.get()
                        .appendUnderlineSpan(
                                imeTextSpans,
                                spannableString.getSpanStart(span),
                                spannableString.getSpanEnd(span));
            } else if (span instanceof SuggestionSpan) {
                final SuggestionSpan suggestionSpan = (SuggestionSpan) span;
                // See android.text.Spanned#SPAN_COMPOSING, We are using this flag to determine if
                // we need to remove the SuggestionSpan after IMEs done with composing state.
                final boolean removeOnFinishComposing = (spanFlags & Spanned.SPAN_COMPOSING) != 0;
                // We support all three flags of SuggestionSpans with caveat:
                // - FLAG_EASY_CORRECT, full support.
                // - FLAG_MISSPELLED, full support.
                // - FLAG_AUTO_CORRECTION, no animation support for this flag for
                //   commitCorrection().
                // Note that FLAG_AUTO_CORRECTION has precedence than the other two flags.

                // Other cases:
                // - Some IMEs (e.g. the AOSP keyboard on Jelly Bean) add SuggestionSpans with no
                //   flags set and no underline color to add suggestions to words marked as
                //   misspelled (instead of having the spell checker return the suggestions when
                //   called). We don't support these either.
                final boolean isEasyCorrectSpan =
                        (suggestionSpan.getFlags() & SuggestionSpan.FLAG_EASY_CORRECT) != 0;
                final boolean isMisspellingSpan =
                        (suggestionSpan.getFlags() & SuggestionSpan.FLAG_MISSPELLED) != 0;
                final boolean isAutoCorrectionSpan =
                        (suggestionSpan.getFlags() & SuggestionSpan.FLAG_AUTO_CORRECTION) != 0;

                if (!isEasyCorrectSpan && !isMisspellingSpan && !isAutoCorrectionSpan) continue;

                // Copied from Android's Editor.java so we use the same colors
                // as the native Android text widget.
                final int underlineColor = getUnderlineColorForSuggestionSpan(suggestionSpan);
                final int newAlpha =
                        (int)
                                (Color.alpha(underlineColor)
                                        * SUGGESTION_HIGHLIGHT_BACKGROUND_TRANSPARENCY);
                final int suggestionHighlightColor =
                        (underlineColor & 0x00FFFFFF) + (newAlpha << 24);

                // In native side, we treat FLAG_AUTO_CORRECTION span as kMisspellingSuggestion
                // marker with 0 suggestion.
                ImeAdapterImplJni.get()
                        .appendSuggestionSpan(
                                imeTextSpans,
                                spannableString.getSpanStart(suggestionSpan),
                                spannableString.getSpanEnd(suggestionSpan),
                                isMisspellingSpan || isAutoCorrectionSpan,
                                removeOnFinishComposing,
                                underlineColor,
                                suggestionHighlightColor,
                                isAutoCorrectionSpan
                                        ? new String[0]
                                        : suggestionSpan.getSuggestions());
            }
        }
    }

    @CalledByNative
    private void cancelComposition() {
        if (DEBUG_LOGS) Log.i(TAG, "cancelComposition");
        if (mInputConnection != null) restartInput();
    }

    @CalledByNative
    @VisibleForTesting
    void onConnectedToRenderProcess() {
        if (DEBUG_LOGS) Log.i(TAG, "onConnectedToRenderProcess");
        mIsConnected = true;
        createInputConnectionFactory();
        resetAndHideKeyboard();
    }

    void performSpellCheck() {
        if (!isValid()) return;
        ImeAdapterImplJni.get().performSpellCheck(mNativeImeAdapterAndroid);
    }

    @NativeMethods
    interface Natives {
        long init(ImeAdapterImpl caller, WebContents webContents);

        boolean sendKeyEvent(
                long nativeImeAdapterAndroid,
                @Nullable KeyEvent event,
                int type,
                int modifiers,
                long timestampMs,
                int keyCode,
                int scanCode,
                boolean isSystemKey,
                int unicodeChar);

        void appendUnderlineSpan(long spanPtr, int start, int end);

        void appendBackgroundColorSpan(long spanPtr, int start, int end, int backgroundColor);

        void appendForegroundColorSpan(long spanPtr, int start, int end, int backgroundColor);

        void appendSuggestionSpan(
                long spanPtr,
                int start,
                int end,
                boolean isMisspelling,
                boolean removeOnFinishComposing,
                int underlineColor,
                int suggestionHighlightColor,
                String[] suggestions);

        void setComposingText(
                long nativeImeAdapterAndroid,
                ImeAdapterImpl self,
                CharSequence text,
                String textStr,
                int newCursorPosition);

        void commitText(
                long nativeImeAdapterAndroid,
                ImeAdapterImpl self,
                CharSequence text,
                String textStr,
                int newCursorPosition);

        void replaceText(
                long nativeImeAdapterAndroid,
                ImeAdapterImpl self,
                int start,
                int end,
                String text,
                int newCursorPosition);

        boolean insertMediaFromURL(long nativeImeAdapterAndroid, String url);

        void finishComposingText(long nativeImeAdapterAndroid);

        void setEditableSelectionOffsets(long nativeImeAdapterAndroid, int start, int end);

        void setComposingRegion(long nativeImeAdapterAndroid, int start, int end);

        void deleteSurroundingText(long nativeImeAdapterAndroid, int before, int after);

        void deleteSurroundingTextInCodePoints(long nativeImeAdapterAndroid, int before, int after);

        boolean requestTextInputStateUpdate(long nativeImeAdapterAndroid);

        void requestCursorUpdate(
                long nativeImeAdapterAndroid, boolean immediateRequest, boolean monitorRequest);

        void advanceFocusForIME(long nativeImeAdapterAndroid, int focusType);

        String[] getSupportedMimeTypes(long nativeImeAdapterAndroid);

        // Stylus Writing
        void handleStylusWritingGestureAction(
                long nativeImeAdapterAndroid, int id, ByteBuffer gestureData);

        void performSpellCheck(long nativeImeAdapterAndroid);
    }
}
