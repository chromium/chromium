// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.app.Activity;
import android.app.SearchManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.os.Handler;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.ActionMode;
import android.view.HapticFeedbackConstants;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.view.textclassifier.SelectionEvent;
import android.view.textclassifier.TextClassifier;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.UserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.content.R;
import org.chromium.content.browser.ContentClassFactory;
import org.chromium.content.browser.GestureListenerManagerImpl;
import org.chromium.content.browser.PopupController;
import org.chromium.content.browser.PopupController.HideablePopup;
import org.chromium.content.browser.WindowEventObserver;
import org.chromium.content.browser.WindowEventObserverManager;
import org.chromium.content.browser.input.ImeAdapterImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.ImeEventObserver;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.SelectAroundCaretResult;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.ViewAndroidDelegate.ContainerViewObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.touch_selection.SelectionEventType;
import org.chromium.ui.touch_selection.TouchSelectionDraggableType;

import java.util.List;

/**
 * Implementation of the interface {@link SelectionPopupController}.
 */
@JNINamespace("content")
public class SelectionPopupControllerImpl extends ActionModeCallbackHelper
        implements ImeEventObserver, SelectionPopupController, WindowEventObserver, HideablePopup,
                   ContainerViewObserver, UserData {
    private static final String TAG = "SelectionPopupCtlr"; // 20 char limit
    private static final boolean DEBUG = false;

    /**
     * Android Intent size limitations prevent sending over a megabyte of data. Limit
     * query lengths to 100kB because other things may be added to the Intent.
     */
    private static final int MAX_SHARE_QUERY_LENGTH = 100000;

    // Default delay for reshowing the {@link ActionMode} after it has been
    // hidden. This avoids flickering issues if there are trailing rect
    // invalidations after the ActionMode is shown. For example, after the user
    // stops dragging a selection handle, in turn showing the ActionMode, the
    // selection change response will be asynchronous. 300ms should accomodate
    // most such trailing, async delays.
    private static final int SHOW_DELAY_MS = 300;

    // A large value to force text processing menu items to be at the end of the
    // context menu. Chosen to be bigger than the order of possible items in the
    // XML template.
    private static final int MENU_ITEM_ORDER_TEXT_PROCESS_START = 100;

    // A flag to determine if we should get readback view from WindowAndroid.
    // The readback view could be the ContainerView, which WindowAndroid has no control on that.
    // Embedders should set this properly to use the correct view for readback.
    private static boolean sShouldGetReadbackViewFromWindowAndroid;

    // A flag to determine if we must only use the context from the associated web contents
    // to inflate menus. By default we use the context held by the ActionMode, because this
    // enables correct theming, but in cases where we rely on the wrapping of contexts for
    // correct resource lookup, this is not correct. In that case we must directly inflate
    // menus from the context.
    private static boolean sMustUseWebContentsContext;

    // Used in tests to disable magnifier.
    private static boolean sDisableMagnifier;

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<SelectionPopupControllerImpl> INSTANCE =
                SelectionPopupControllerImpl::new;
    }

    private final Handler mHandler;
    private Context mContext;
    private WindowAndroid mWindowAndroid;
    private WebContentsImpl mWebContents;
    private ActionMode.Callback2 mCallback;
    private RenderFrameHost mRenderFrameHost;
    private long mNativeSelectionPopupController;

    private SelectionClient.ResultCallback mResultCallback;

    // Used to customize PastePopupMenu
    private ActionMode.Callback mNonSelectionCallback;

    // Selection rectangle in DIP.
    private final Rect mSelectionRect = new Rect();

    // Self-repeating task that repeatedly hides the ActionMode. This is
    // required because ActionMode only exposes a temporary hide routine.
    private Runnable mRepeatingHideRunnable;

    // Can be null temporarily when switching between WindowAndroid.
    @Nullable
    private View mView;
    private ActionMode mActionMode;

    // Supplier of whether action bar is showing now.
    private final ObservableSupplierImpl<Boolean> mIsActionBarShowingSupplier =
            new ObservableSupplierImpl<>();

    // Bit field for mappings from menu item to a flag indicating it is allowed.
    private int mAllowedMenuItems;

    private boolean mHidden;

    private boolean mEditable;
    private boolean mIsPasswordType;
    private boolean mIsInsertionForTesting;
    private boolean mCanSelectAllForPastePopup;
    private boolean mCanEditRichly;

    private boolean mUnselectAllOnDismiss;
    private String mLastSelectedText;
    private int mLastSelectionOffset;
    private boolean mIsInHandleDragging;

    // Tracks whether a touch selection is currently active.
    private boolean mHasSelection;

    // Lazily created paste popup menu, triggered either via long press in an
    // editable region or from tapping the insertion handle.
    private PastePopupMenu mPastePopupMenu;
    private boolean mWasPastePopupShowingOnInsertionDragStart;

    /**
     * The {@link SelectionClient} that processes textual selection, or {@code null} if none
     * exists.
     */
    private SelectionClient mSelectionClient;

    @Nullable
    private SmartSelectionEventProcessor mSmartSelectionEventProcessor;

    private PopupController mPopupController;

    // The classificaton result of the selected text if the selection exists and
    // SelectionClient was able to classify it, otherwise null.
    private SelectionClient.Result mClassificationResult;

    private boolean mPreserveSelectionOnNextLossOfFocus;

    private MagnifierAnimator mMagnifierAnimator;

    private AdditionalMenuItemProvider mAdditionalMenuItemProvider;

    /**
     * An interface for getting {@link View} for readback.
     */
    public interface ReadbackViewCallback {
        /**
         * Gets the {@link View} for readback.
         */
        View getReadbackView();
    }

    /**
     * Sets to use the readback view from {@link WindowAndroid}.
     */
    public static void setShouldGetReadbackViewFromWindowAndroid() {
        sShouldGetReadbackViewFromWindowAndroid = true;
    }

    public static void setMustUseWebContentsContext() {
        sMustUseWebContentsContext = true;
    }

    /**
     * Get {@link SelectionPopupController} object used for the give WebContents.
     * {@link #create()} should precede any calls to this.
     * @param webContents {@link WebContents} object.
     * @return {@link SelectionPopupController} object. {@code null} if not available because
     *         {@link #create()} is not called yet.
     */
    public static SelectionPopupControllerImpl fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(
                        SelectionPopupControllerImpl.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    /**
     * Get {@link SelectionPopupController} object used for the given WebContents but does not
     * create a new one.
     * @param webContents {@link WebContents} object.
     * @return {@link SelectionPopupController} object. {@code null} if not available.
     */
    public static SelectionPopupControllerImpl fromWebContentsNoCreate(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(SelectionPopupControllerImpl.class, null);
    }

    /**
     * Create {@link SelectionPopupController} instance. Note that it will create an instance with
     * no link to native side for testing only.
     * @param webContents {@link WebContents} mocked for testing.
     * @param popupController {@link PopupController} mocked for testing.
     */
    public static SelectionPopupControllerImpl createForTesting(
            WebContents webContents, PopupController popupController) {
        return new SelectionPopupControllerImpl(webContents, popupController, false);
    }

    public static SelectionPopupControllerImpl createForTesting(WebContents webContents) {
        return new SelectionPopupControllerImpl(webContents, null, false);
    }

    @VisibleForTesting
    public static void setDisableMagnifierForTesting(boolean disable) {
        sDisableMagnifier = disable;
    }

    /**
     * Create {@link SelectionPopupControllerImpl} instance.
     * @param webContents WebContents instance.
     */
    public SelectionPopupControllerImpl(WebContents webContents) {
        this(webContents, null, true);
        setActionModeCallback(ActionModeCallbackHelper.EMPTY_CALLBACK);
    }

    private SelectionPopupControllerImpl(
            WebContents webContents, PopupController popupController, boolean initializeNative) {
        mHandler = new Handler();
        mWebContents = (WebContentsImpl) webContents;
        mPopupController = popupController;
        mContext = mWebContents.getContext();
        mWindowAndroid = mWebContents.getTopLevelNativeWindow();
        ViewAndroidDelegate viewDelegate = mWebContents.getViewAndroidDelegate();
        if (viewDelegate != null) {
            mView = viewDelegate.getContainerView();
            viewDelegate.addObserver(this);
        }

        // The menu items are allowed by default.
        mAllowedMenuItems = MENU_ITEM_SHARE | MENU_ITEM_WEB_SEARCH | MENU_ITEM_PROCESS_TEXT;
        mRepeatingHideRunnable = new Runnable() {
            @Override
            public void run() {
                assert mHidden;
                final long hideDuration = getDefaultHideDuration();
                // Ensure the next hide call occurs before the ActionMode reappears.
                mHandler.postDelayed(mRepeatingHideRunnable, hideDuration - 1);
                hideActionModeTemporarily(hideDuration);
            }
        };

        WindowEventObserverManager manager = WindowEventObserverManager.from(mWebContents);
        if (manager != null) {
            manager.addObserver(this);
        }
        if (initializeNative) {
            mNativeSelectionPopupController = SelectionPopupControllerImplJni.get().init(
                    SelectionPopupControllerImpl.this, mWebContents);
            ImeAdapterImpl imeAdapter = ImeAdapterImpl.fromWebContents(mWebContents);
            if (imeAdapter != null) imeAdapter.addEventObserver(this);
        }

        mResultCallback = new SmartSelectionCallback();
        mLastSelectedText = "";
        initMagnifier();
        mAdditionalMenuItemProvider = ContentClassFactory.get().createAddtionalMenuItemProvider();
        getPopupController().registerPopup(this);
    }

    private void reset() {
        dropFocus();
        mContext = null;
        mWindowAndroid = null;
    }

    private void dropFocus() {
        // Hide popups and clear selection.
        destroyActionModeAndUnselect();
        dismissTextHandles();
        PopupController.hideAll(mWebContents);
        // Clear the selection. The selection is cleared on destroying IME
        // and also here since we may receive destroy first, for example
        // when focus is lost in webview.
        clearSelection();
    }

    public static String sanitizeQuery(String query, int maxLength) {
        if (TextUtils.isEmpty(query) || query.length() < maxLength) return query;
        Log.w(TAG, "Truncating oversized query (" + query.length() + ").");
        return query.substring(0, maxLength) + "…";
    }

    // ViewAndroidDelegate.ContainerViewObserver

    @Override
    public void onUpdateContainerView(ViewGroup view) {
        // Cleans up action mode before switching to a new container view.
        if (isActionModeValid()) finishActionMode();
        mUnselectAllOnDismiss = true;
        destroyPastePopup();

        if (view != null) view.setClickable(true);
        mView = view;
        initMagnifier();
    }

    // ImeEventObserver

    @Override
    public void onNodeAttributeUpdated(boolean editable, boolean password) {
        updateSelectionState(editable, password);
    }

    @Override
    public void setActionModeCallback(ActionMode.Callback2 callback) {
        mCallback = callback;
    }

    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    @Override
    public void setNonSelectionActionModeCallback(ActionMode.Callback callback) {
        mNonSelectionCallback = callback;
    }

    @Override
    public SelectionClient.ResultCallback getResultCallback() {
        return mResultCallback;
    }

    public SelectionClient.Result getClassificationResult() {
        return mClassificationResult;
    }

    /**
     * Gets the current {@link SelectionClient}.
     */
    public SelectionClient getSelectionClient() {
        return mSelectionClient;
    }

    @Override
    public boolean isActionModeValid() {
        return mActionMode != null;
    }

    // True if action mode is initialized to a working (not a no-op) mode.
    @VisibleForTesting
    public boolean isActionModeSupported() {
        return mCallback != EMPTY_CALLBACK;
    }

    @Override
    public void setAllowedMenuItems(int allowedMenuItems) {
        mAllowedMenuItems = allowedMenuItems;
    }

    @Override
    public int getAllowedMenuItemIfAny(ActionMode mode, MenuItem item) {
        if (!isActionModeValid()) return 0;

        int id = item.getItemId();
        int groupId = item.getGroupId();
        if (id == R.id.select_action_menu_share) {
            return MENU_ITEM_SHARE;
        } else if (id == R.id.select_action_menu_web_search) {
            return MENU_ITEM_WEB_SEARCH;
        } else if (groupId == R.id.select_action_menu_text_processing_menus) {
            return MENU_ITEM_PROCESS_TEXT;
        }
        return 0;
    }

    @VisibleForTesting
    @CalledByNative
    public void showSelectionMenu(int left, int top, int right, int bottom, int handleHeight,
            boolean isEditable, boolean isPasswordType, String selectionText,
            int selectionStartOffset, boolean canSelectAll, boolean canRichlyEdit,
            boolean shouldSuggest, @MenuSourceType int sourceType,
            RenderFrameHost renderFrameHost) {
        int offsetBottom = bottom;
        offsetBottom += handleHeight;
        mSelectionRect.set(left, top, right, offsetBottom);
        mEditable = isEditable;
        mLastSelectedText = selectionText;
        mLastSelectionOffset = selectionStartOffset;
        mHasSelection = selectionText.length() != 0;
        mIsPasswordType = isPasswordType;
        mCanSelectAllForPastePopup = canSelectAll;
        mCanEditRichly = canRichlyEdit;
        mUnselectAllOnDismiss = true;

        if (hasSelection()) {
            mRenderFrameHost = renderFrameHost;

            if (mSmartSelectionEventProcessor != null) {
                switch (sourceType) {
                    case MenuSourceType.MENU_SOURCE_ADJUST_SELECTION:
                        mSmartSelectionEventProcessor.onSelectionModified(
                                mLastSelectedText, mLastSelectionOffset, mClassificationResult);
                        break;
                    case MenuSourceType.MENU_SOURCE_ADJUST_SELECTION_RESET:
                        mSmartSelectionEventProcessor.onSelectionAction(mLastSelectedText,
                                mLastSelectionOffset, SelectionEvent.ACTION_RESET,
                                /* SelectionClient.Result = */ null);
                        break;
                    case MenuSourceType.MENU_SOURCE_TOUCH_HANDLE:
                        break;
                    default:
                        mSmartSelectionEventProcessor.onSelectionStarted(
                                mLastSelectedText, mLastSelectionOffset, isEditable);
                }
            }

            // From selection adjustment, show menu directly.
            // Note that this won't happen if it is incognito mode or device is not provisioned.
            if (sourceType == MenuSourceType.MENU_SOURCE_ADJUST_SELECTION) {
                showActionModeOrClearOnFailure();
                return;
            }

            // Show menu there is no updates from SelectionClient.
            if (mSelectionClient == null
                    || !mSelectionClient.requestSelectionPopupUpdates(shouldSuggest)) {
                showActionModeOrClearOnFailure();
            }
        } else {
            createAndShowPastePopup();
        }
    }

    /**
     * Show (activate) android action mode by starting it.
     *
     * <p>Action mode in floating mode is tried first, and then falls back to
     * a normal one.
     * <p> If the action mode cannot be created the selection is cleared.
     */
    public void showActionModeOrClearOnFailure() {
        if (!isActionModeSupported() || !hasSelection() || mView == null) return;

        // Just refresh non-floating action mode if it already exists to avoid blinking.
        if (isActionModeValid() && !isFloatingActionMode()) {
            // Try/catch necessary for framework bug, crbug.com/446717.
            try {
                mActionMode.invalidate();
            } catch (NullPointerException e) {
                Log.w(TAG, "Ignoring NPE from ActionMode.invalidate() as workaround for L", e);
            }
            hideActionMode(false);
            return;
        }

        // Reset overflow menu (see crbug.com/700929).
        destroyActionModeAndKeepSelection();

        assert mWebContents != null;
        ActionMode actionMode = mView.startActionMode(mCallback, ActionMode.TYPE_FLOATING);
        if (actionMode != null) {
            // This is to work around an LGE email issue. See crbug.com/651706 for more details.
            LGEmailActionModeWorkaroundImpl.runIfNecessary(mContext, actionMode);
        }
        setActionMode(actionMode);
        mUnselectAllOnDismiss = true;

        if (!isActionModeValid()) clearSelection();
    }

    private void dismissTextHandles() {
        if (mWebContents.getRenderWidgetHostView() != null) {
            mWebContents.getRenderWidgetHostView().dismissTextHandles();
        }
    }

    private void showContextMenuAtTouchHandle(int left, int bottom) {
        if (mWebContents.getRenderWidgetHostView() != null) {
            mWebContents.getRenderWidgetHostView().showContextMenuAtTouchHandle(left, bottom);
        }
    }

    private void createAndShowPastePopup() {
        if (mView == null || mView.getParent() == null || mView.getVisibility() != View.VISIBLE) {
            return;
        }

        destroyPastePopup();
        PastePopupMenu.PastePopupMenuDelegate delegate =
                new PastePopupMenu.PastePopupMenuDelegate() {
                    @Override
                    public void paste() {
                        SelectionPopupControllerImpl.this.paste();
                        dismissTextHandles();
                    }

                    @Override
                    public void pasteAsPlainText() {
                        SelectionPopupControllerImpl.this.pasteAsPlainText();
                        dismissTextHandles();
                    }

                    @Override
                    public boolean canPaste() {
                        return Clipboard.getInstance().canPaste();
                    }

                    @Override
                    public void selectAll() {
                        SelectionPopupControllerImpl.this.selectAll();
                    }

                    @Override
                    public boolean canSelectAll() {
                        return SelectionPopupControllerImpl.this.canSelectAll();
                    }

                    @Override
                    public boolean canPasteAsPlainText() {
                        return SelectionPopupControllerImpl.this.canPasteAsPlainText();
                    }
                };
        Context windowContext = mWindowAndroid.getContext().get();
        if (windowContext == null) return;
        mPastePopupMenu =
                new FloatingPastePopupMenu(windowContext, mView, delegate, mNonSelectionCallback);
        showPastePopup();
    }

    private void showPastePopup() {
        try {
            mPastePopupMenu.show(getSelectionRectRelativeToContainingView());
        } catch (WindowManager.BadTokenException e) {
        }
    }

    // HideablePopup implementation
    @Override
    public void hide() {
        destroyPastePopup();
    }

    public void destroyPastePopup() {
        if (isPastePopupShowing()) {
            mPastePopupMenu.hide();
            mPastePopupMenu = null;
        }
    }

    @VisibleForTesting
    public boolean isPastePopupShowing() {
        return mPastePopupMenu != null;
    }

    // Composition methods for android.view.ActionMode

    /**
     * @see ActionMode#finish()
     */
    @Override
    public void finishActionMode() {
        mHidden = false;
        mHandler.removeCallbacks(mRepeatingHideRunnable);

        if (isActionModeValid()) {
            mActionMode.finish();

            // Should be nulled out in case #onDestroyActionMode() is not invoked in response.
            setActionMode(null);
        }
    }

    /**
     * @see ActionMode#invalidateContentRect()
     */
    public void invalidateContentRect() {
        if (isActionModeValid()) ApiHelperForM.invalidateContentRectOnActionMode(mActionMode);
    }

    // WindowEventObserver

    @Override
    public void onWindowFocusChanged(boolean gainFocus) {
        if (isActionModeValid()) {
            ApiHelperForM.onWindowFocusChangedOnActionMode(mActionMode, gainFocus);
        }
    }

    @Override
    public void onAttachedToWindow() {
        updateTextSelectionUI(true);
    }

    @Override
    public void onDetachedFromWindow() {
        // WebView uses PopupWindows for handle rendering, which may remain
        // unintentionally visible even after the WebView has been detached.
        // Override the handle visibility explicitly to address this, but
        // preserve the underlying selection for detachment cases like screen
        // locking and app switching.
        updateTextSelectionUI(false);
    }

    @Override
    public void onWindowAndroidChanged(WindowAndroid newWindowAndroid) {
        if (newWindowAndroid == null) {
            reset();
            return;
        }

        mWindowAndroid = newWindowAndroid;
        mContext = mWebContents.getContext();
        initMagnifier();
        destroyPastePopup();
    }

    @Override
    public void onRotationChanged(int rotation) {
        // ActionMode#invalidate() won't be able to re-layout the floating
        // action mode menu items according to the new rotation. So Chrome
        // has to re-create the action mode.
        if (isActionModeValid()) {
            hidePopupsAndPreserveSelection();
            showActionModeOrClearOnFailure();
        }
    }

    @Override
    public void onViewFocusChanged(boolean gainFocus, boolean hideKeyboardOnBlur) {
        if (gainFocus) {
            restoreSelectionPopupsIfNecessary();
        } else {
            ImeAdapterImpl.fromWebContents(mWebContents)
                    .cancelRequestToScrollFocusedEditableNodeIntoView();
            if (getPreserveSelectionOnNextLossOfFocus()) {
                setPreserveSelectionOnNextLossOfFocus(false);
                hidePopupsAndPreserveSelection();
            } else {
                dropFocus();
            }
        }
    }

    /**
     * Update scroll status.
     * @param scrollInProgress {@code true} if scroll is in progress.
     */
    public void setScrollInProgress(boolean scrollInProgress) {
        hideActionMode(scrollInProgress);
    }

    /**
     * Hide or reveal the ActionMode. Note that this only has visible
     * side-effects if the underlying ActionMode supports hiding.
     * @param hide whether to hide or show the ActionMode.
     */
    private void hideActionMode(boolean hide) {
        if (!isFloatingActionMode()) return;
        if (mHidden == hide) return;
        mHidden = hide;
        if (mHidden) {
            mRepeatingHideRunnable.run();
        } else {
            mHandler.removeCallbacks(mRepeatingHideRunnable);
            // To show the action mode that is being hidden call hide() again with a short delay.
            hideActionModeTemporarily(SHOW_DELAY_MS);
        }
    }

    /**
     * @see ActionMode#hide(long)
     */
    private void hideActionModeTemporarily(long duration) {
        assert isFloatingActionMode();
        if (isActionModeValid()) ApiHelperForM.hideActionMode(mActionMode, duration);
    }

    private boolean isFloatingActionMode() {
        return isActionModeValid()
                && ApiHelperForM.getActionModeType(mActionMode) == ActionMode.TYPE_FLOATING;
    }

    private long getDefaultHideDuration() {
        return ApiHelperForM.getDefaultActionModeHideDuration();
    }

    // Default handlers for action mode callbacks.

    @Override
    public void onCreateActionMode(ActionMode mode, Menu menu) {
        mode.setTitle(DeviceFormFactor.isWindowOnTablet(mWindowAndroid)
                        ? mContext.getString(R.string.actionbar_textselection_title)
                        : null);
        mode.setSubtitle(null);
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        if (mAdditionalMenuItemProvider != null) {
            mAdditionalMenuItemProvider.clearMenuItemListeners();
        }
        // Only remove action mode items we added. See more http://crbug.com/709878.
        menu.removeGroup(R.id.select_action_menu_default_items);
        menu.removeGroup(R.id.select_action_menu_assist_items);
        menu.removeGroup(R.id.select_action_menu_text_processing_menus);
        menu.removeGroup(android.R.id.textAssist);
        createActionMenu(mode, menu);
        return true;
    }

    /**
     * Initialize the menu by populating all the available items. Embedders should remove
     * the items that are not relevant to the input text being edited.
     */
    public static void initializeMenu(Context context, ActionMode mode, Menu menu) {
        if (!sMustUseWebContentsContext) {
            // For WebView the correct choice is to use the actionMode context because webview
            // assets have been added to its asset path. However we need to fall back to
            // using the web contents context in the case where the AssetManager associated with
            // the actionMode context does not contain our assets, because not doing so will cause a
            // crash.
            try {
                mode.getMenuInflater().inflate(R.menu.select_action_menu, menu);
                return;
            } catch (Resources.NotFoundException e) {
                // TODO(tobiasjs) by the time we get here we have already
                // caused a resource loading failure to be logged. WebView
                // resource access needs to be improved so that this
                // logspam can be avoided.
            }
        }
        new MenuInflater(context).inflate(R.menu.select_action_menu, menu);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    public static void setPasteAsPlainTextMenuItemTitle(Menu menu) {
        MenuItem item = menu.findItem(R.id.select_action_menu_paste_as_plain_text);
        if (item == null) return;
        // android.R.string.paste_as_plain_text is available in SDK since O.
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        item.setTitle(android.R.string.paste_as_plain_text);
    }

    private void createActionMenu(ActionMode mode, Menu menu) {
        initializeMenu(mContext, mode, menu);
        updateAssistMenuItem(menu);
        removeActionMenuItemsIfNecessary(menu);
        setPasteAsPlainTextMenuItemTitle(menu);

        Context windowContext = mWindowAndroid.getContext().get();
        if (mClassificationResult != null && mAdditionalMenuItemProvider != null
                && windowContext != null) {
            mAdditionalMenuItemProvider.addMenuItems(windowContext, menu,
                    mClassificationResult.textClassification,
                    mClassificationResult.additionalIcons);
        }

        if (!hasSelection() || isSelectionPassword()) return;

        initializeTextProcessingMenu(menu);
    }

    private void removeActionMenuItemsIfNecessary(Menu menu) {
        if (!isFocusedNodeEditable() || !Clipboard.getInstance().canPaste()) {
            menu.removeItem(R.id.select_action_menu_paste);
            menu.removeItem(R.id.select_action_menu_paste_as_plain_text);
        }

        if (!canPasteAsPlainText()) {
            menu.removeItem(R.id.select_action_menu_paste_as_plain_text);
        }

        if (!hasSelection()) {
            menu.removeItem(R.id.select_action_menu_select_all);
            menu.removeItem(R.id.select_action_menu_cut);
            menu.removeItem(R.id.select_action_menu_copy);
            menu.removeItem(R.id.select_action_menu_share);
            menu.removeItem(R.id.select_action_menu_web_search);
            return;
        }

        if (!isFocusedNodeEditable()) {
            menu.removeItem(R.id.select_action_menu_cut);
        }

        if (isFocusedNodeEditable() || !isSelectActionModeAllowed(MENU_ITEM_SHARE)) {
            menu.removeItem(R.id.select_action_menu_share);
        }

        if (isFocusedNodeEditable() || isIncognito()
                || !isSelectActionModeAllowed(MENU_ITEM_WEB_SEARCH)) {
            menu.removeItem(R.id.select_action_menu_web_search);
        }

        if (isSelectionPassword() || !Clipboard.getInstance().canCopy()) {
            menu.removeItem(R.id.select_action_menu_copy);
            menu.removeItem(R.id.select_action_menu_cut);
        }
    }

    /**
     * Check if need to show "paste as plain text" option.
     * "paste as plain text" option needs clibpoard content is rich text, and editor supports rich
     * text as well.
     */
    @VisibleForTesting
    public boolean canPasteAsPlainText() {
        // String resource "paste_as_plain_text" only exist in O+.
        // Also this is an O feature, we need to make it consistent with TextView.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false;
        if (!mCanEditRichly) return false;

        // We need to show "paste as plain text" when Clipboard contains the HTML text. In addition
        // to that, on Android, Spanned could be copied to Clipboard as plain_text MIME type, but in
        // some cases, Spanned could have text format, we need to show "paste as plain text" when
        // that happens as well.
        return Clipboard.getInstance().hasHTMLOrStyledText();
    }

    private void updateAssistMenuItem(Menu menu) {
        // There is no Assist functionality before Android O.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return;

        if (mClassificationResult != null && mClassificationResult.hasNamedAction()) {
            menu.add(R.id.select_action_menu_assist_items, android.R.id.textAssist, 1,
                        mClassificationResult.label)
                    .setIcon(mClassificationResult.icon);
        }
    }

    /**
     * Intialize the menu items for processing text, if there is any.
     */
    @VisibleForTesting
    /* package */ void initializeTextProcessingMenu(Menu menu) {
        if (!isSelectActionModeAllowed(MENU_ITEM_PROCESS_TEXT)) {
            return;
        }

        List<ResolveInfo> supportedActivities =
                PackageManagerUtils.queryIntentActivities(createProcessTextIntent(), 0);
        for (int i = 0; i < supportedActivities.size(); i++) {
            ResolveInfo resolveInfo = supportedActivities.get(i);
            if (resolveInfo.activityInfo == null || !resolveInfo.activityInfo.exported) continue;

            CharSequence label = resolveInfo.loadLabel(mContext.getPackageManager());
            menu.add(R.id.select_action_menu_text_processing_menus, Menu.NONE,
                        MENU_ITEM_ORDER_TEXT_PROCESS_START + i, label)
                    .setIntent(createProcessTextIntentForResolveInfo(resolveInfo))
                    .setShowAsAction(MenuItem.SHOW_AS_ACTION_IF_ROOM);
        }
    }

    private static Intent createProcessTextIntent() {
        return new Intent().setAction(Intent.ACTION_PROCESS_TEXT).setType("text/plain");
    }

    private Intent createProcessTextIntentForResolveInfo(ResolveInfo info) {
        boolean isReadOnly = !isFocusedNodeEditable();
        return createProcessTextIntent()
                .putExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, isReadOnly)
                .setClassName(info.activityInfo.packageName, info.activityInfo.name);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        // Actions should only happen when there is a WindowAndroid so mView should not be null.
        assert mView != null;
        if (!isActionModeValid()) return true;

        int id = item.getItemId();
        int groupId = item.getGroupId();

        if (hasSelection() && mSmartSelectionEventProcessor != null) {
            mSmartSelectionEventProcessor.onSelectionAction(mLastSelectedText, mLastSelectionOffset,
                    getActionType(id, groupId), mClassificationResult);
        }

        if (groupId == R.id.select_action_menu_assist_items && id == android.R.id.textAssist) {
            doAssistAction();
            mode.finish();
        } else if (id == R.id.select_action_menu_select_all) {
            selectAll();
        } else if (id == R.id.select_action_menu_cut) {
            cut();
            mode.finish();
        } else if (id == R.id.select_action_menu_copy) {
            copy();
            mode.finish();
        } else if (id == R.id.select_action_menu_paste) {
            paste();
            mode.finish();
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && id == R.id.select_action_menu_paste_as_plain_text) {
            pasteAsPlainText();
            mode.finish();
        } else if (id == R.id.select_action_menu_share) {
            share();
            mode.finish();
        } else if (id == R.id.select_action_menu_web_search) {
            search();
            mode.finish();
        } else if (groupId == R.id.select_action_menu_text_processing_menus) {
            processText(item.getIntent());
            // The ActionMode is not dismissed to match the behavior with
            // TextView in Android M.
        } else if (groupId == android.R.id.textAssist) {
            if (mAdditionalMenuItemProvider != null) {
                mAdditionalMenuItemProvider.performAction(item, mView);
                mode.finish();
            }
        } else {
            return false;
        }

        return true;
    }

    @Override
    public void onDestroyActionMode() {
        setActionMode(null);
        if (mUnselectAllOnDismiss) {
            clearSelection();
        }
    }

    /**
     * Called when an ActionMode needs to be positioned on screen, potentially occluding view
     * content. Note this may be called on a per-frame basis.
     *
     * @param mode The ActionMode that requires positioning.
     * @param view The View that originated the ActionMode, in whose coordinates the Rect should
     *             be provided.
     * @param outRect The Rect to be populated with the content position.
     */
    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        outRect.set(getSelectionRectRelativeToContainingView());
    }

    private Rect getSelectionRectRelativeToContainingView() {
        float deviceScale = getDeviceScaleFactor();
        Rect viewSelectionRect = new Rect((int) (mSelectionRect.left * deviceScale),
                (int) (mSelectionRect.top * deviceScale),
                (int) (mSelectionRect.right * deviceScale),
                (int) (mSelectionRect.bottom * deviceScale));

        // The selection coordinates are relative to the content viewport, but we need
        // coordinates relative to the containing View.
        viewSelectionRect.offset(
                0, (int) mWebContents.getRenderCoordinates().getContentOffsetYPix());
        return viewSelectionRect;
    }

    private float getDeviceScaleFactor() {
        return mWebContents.getRenderCoordinates().getDeviceScaleFactor();
    }

    private int getActionType(int menuItemId, int menuItemGroupId) {
        if (menuItemGroupId == android.R.id.textAssist) {
            return SelectionEvent.ACTION_SMART_SHARE;
        }
        if (menuItemId == R.id.select_action_menu_select_all) {
            return SelectionEvent.ACTION_SELECT_ALL;
        }
        if (menuItemId == R.id.select_action_menu_cut) {
            return SelectionEvent.ACTION_CUT;
        }
        if (menuItemId == R.id.select_action_menu_copy) {
            return SelectionEvent.ACTION_COPY;
        }
        if (menuItemId == R.id.select_action_menu_paste
                || menuItemId == R.id.select_action_menu_paste_as_plain_text) {
            return SelectionEvent.ACTION_PASTE;
        }
        if (menuItemId == R.id.select_action_menu_share) {
            return SelectionEvent.ACTION_SHARE;
        }
        if (menuItemId == android.R.id.textAssist) {
            return SelectionEvent.ACTION_SMART_SHARE;
        }
        return SelectionEvent.ACTION_OTHER;
    }

    /**
     * Perform an action that depends on the semantics of the selected text.
     */
    @VisibleForTesting
    void doAssistAction() {
        assert mView != null;
        if (mClassificationResult == null || !mClassificationResult.hasNamedAction()) return;

        assert mClassificationResult.onClickListener != null
                || mClassificationResult.intent != null;

        if (mClassificationResult.onClickListener != null) {
            mClassificationResult.onClickListener.onClick(mView);
            return;
        }

        if (mClassificationResult.intent != null) {
            Context context = mWindowAndroid.getContext().get();
            if (context == null) return;

            context.startActivity(mClassificationResult.intent);
            return;
        }
    }

    /**
     * Perform a select all action.
     */
    @VisibleForTesting
    public void selectAll() {
        mWebContents.selectAll();
        mClassificationResult = null;
        // Even though the above statement logged a SelectAll user action, we want to
        // track whether the focus was in an editable field, so log that too.
        if (isFocusedNodeEditable()) {
            RecordUserAction.record("MobileActionMode.SelectAllWasEditable");
        } else {
            RecordUserAction.record("MobileActionMode.SelectAllWasNonEditable");
        }
    }

    /**
     * Perform a cut (to clipboard) action.
     */
    @VisibleForTesting
    public void cut() {
        mWebContents.cut();
    }

    /**
     * Perform a copy (to clipboard) action.
     */
    @VisibleForTesting
    public void copy() {
        mWebContents.copy();
    }

    /**
     * Perform a paste action.
     */
    @VisibleForTesting
    public void paste() {
        mWebContents.paste();
    }

    /**
     * Perform a paste as plain text action.
     */
    @VisibleForTesting
    void pasteAsPlainText() {
        mWebContents.pasteAsPlainText();
    }

    /**
     * Perform a share action.
     */
    @VisibleForTesting
    public void share() {
        RecordUserAction.record(UMA_MOBILE_ACTION_MODE_SHARE);
        String query = sanitizeQuery(getSelectedText(), MAX_SHARE_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_TEXT, query);
        try {
            Intent i = Intent.createChooser(send, mContext.getString(R.string.actionbar_share));
            i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(i);
        } catch (android.content.ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }

    /**
     * Perform a processText action (translating the text, for example).
     */
    private void processText(Intent intent) {
        RecordUserAction.record("MobileActionMode.ProcessTextIntent");

        // Use MAX_SHARE_QUERY_LENGTH for the Intent 100k limitation.
        String query = sanitizeQuery(getSelectedText(), MAX_SHARE_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        intent.putExtra(Intent.EXTRA_PROCESS_TEXT, query);

        // Intent is sent by WindowAndroid by default.
        try {
            mWindowAndroid.showIntent(intent, new WindowAndroid.IntentCallback() {
                @Override
                public void onIntentCompleted(int resultCode, Intent data) {
                    onReceivedProcessTextResult(resultCode, data);
                }
            }, null);
        } catch (android.content.ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }

    /**
     * Perform a search action.
     */
    @VisibleForTesting
    public void search() {
        RecordUserAction.record("MobileActionMode.WebSearch");
        String query = sanitizeQuery(getSelectedText(), MAX_SEARCH_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        Intent i = new Intent(Intent.ACTION_WEB_SEARCH);
        i.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
        i.putExtra(SearchManager.QUERY, query);
        i.putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName());
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try {
            mContext.startActivity(i);
        } catch (android.content.ActivityNotFoundException ex) {
            // If no app handles it, do nothing.
        }
    }

    /**
     * @return true if the current selection is of password type.
     */
    @VisibleForTesting
    public boolean isSelectionPassword() {
        return mIsPasswordType;
    }

    @Override
    public boolean isFocusedNodeEditable() {
        return mEditable;
    }

    /**
     * @return true if the current selection is an insertion point.
     */
    @VisibleForTesting
    public boolean isInsertionForTesting() {
        return mIsInsertionForTesting;
    }

    /**
     * @return true if the current selection can select all.
     */
    @VisibleForTesting
    public boolean canSelectAll() {
        return mCanSelectAllForPastePopup;
    }

    /**
     * @return true if the current selection is for incognito content.
     *         Note: This should remain constant for the callback's lifetime.
     */
    private boolean isIncognito() {
        return mWebContents.isIncognito();
    }

    /**
     * @param actionModeItem the flag for the action mode item in question. The valid flags are
     *        {@link #MENU_ITEM_SHARE}, {@link #MENU_ITEM_WEB_SEARCH}, and
     *        {@link #MENU_ITEM_PROCESS_TEXT}.
     * @return true if the menu item action is allowed. Otherwise, the menu item
     *         should be removed from the menu.
     */
    private boolean isSelectActionModeAllowed(int actionModeItem) {
        boolean isAllowedByClient = (mAllowedMenuItems & actionModeItem) != 0;
        if (actionModeItem == MENU_ITEM_SHARE) {
            return isAllowedByClient && isShareAvailable();
        }
        return isAllowedByClient;
    }

    @Override
    public void onReceivedProcessTextResult(int resultCode, Intent data) {
        if (mWebContents == null || resultCode != Activity.RESULT_OK || data == null) return;

        // Do not handle the result if no text is selected or current selection is not editable.
        if (!hasSelection() || !isFocusedNodeEditable()) return;

        CharSequence result = data.getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT);
        if (result != null) {
            // TODO(hush): Use a variant of replace that re-selects the replaced text.
            // crbug.com/546710
            mWebContents.replace(result.toString());
        }
    }

    @Override
    public void setPreserveSelectionOnNextLossOfFocus(boolean preserve) {
        mPreserveSelectionOnNextLossOfFocus = preserve;
    }

    public boolean getPreserveSelectionOnNextLossOfFocus() {
        return mPreserveSelectionOnNextLossOfFocus;
    }

    @Override
    public void updateTextSelectionUI(boolean focused) {
        setTextHandlesTemporarilyHidden(!focused);
        if (focused) {
            restoreSelectionPopupsIfNecessary();
        } else {
            destroyActionModeAndKeepSelection();
            getPopupController().hideAllPopups();
        }
    }

    private void setTextHandlesTemporarilyHidden(boolean hide) {
        if (mNativeSelectionPopupController == 0) return;
        SelectionPopupControllerImplJni.get().setTextHandlesTemporarilyHidden(
                mNativeSelectionPopupController, SelectionPopupControllerImpl.this, hide);
    }

    @CalledByNative
    public void restoreSelectionPopupsIfNecessary() {
        if (hasSelection() && !isActionModeValid()) {
            showActionModeOrClearOnFailure();
        }
    }

    // All coordinates are in DIP.
    @VisibleForTesting
    @CalledByNative
    void onSelectionEvent(
            @SelectionEventType int eventType, int left, int top, int right, int bottom) {
        if (DEBUG) {
            Log.i(TAG,
                    "onSelectionEvent: " + eventType + "[(" + left + ", " + top + ")-(" + right
                            + ", " + bottom + ")]");
        }
        // Ensure the provided selection coordinates form a non-empty rect, as required by
        // the selection action mode.
        // NOTE: the native side ensures the rectangle is not empty, but that's done using floating
        // point, which means it's entirely possible for this code to receive an empty rect.
        if (left == right) ++right;
        if (top == bottom) ++bottom;

        switch (eventType) {
            case SelectionEventType.SELECTION_HANDLES_SHOWN:
                mSelectionRect.set(left, top, right, bottom);
                break;

            case SelectionEventType.SELECTION_HANDLES_MOVED:
                mSelectionRect.set(left, top, right, bottom);
                invalidateContentRect();
                if (mIsInHandleDragging) {
                    performHapticFeedback();
                }
                break;

            case SelectionEventType.SELECTION_HANDLES_CLEARED:
                mLastSelectedText = "";
                mLastSelectionOffset = 0;
                mHasSelection = false;
                mUnselectAllOnDismiss = false;
                mSelectionRect.setEmpty();
                if (mSelectionClient != null) mSelectionClient.cancelAllRequests();

                mRenderFrameHost = null;
                finishActionMode();
                break;

            case SelectionEventType.SELECTION_HANDLE_DRAG_STARTED:
                hideActionMode(true);
                mIsInHandleDragging = true;
                break;

            case SelectionEventType.SELECTION_HANDLE_DRAG_STOPPED:
                showContextMenuAtTouchHandle(left, bottom);
                if (mMagnifierAnimator != null) {
                    mMagnifierAnimator.handleDragStopped();
                }
                mIsInHandleDragging = false;
                break;

            case SelectionEventType.INSERTION_HANDLE_SHOWN:
                mSelectionRect.set(left, top, right, bottom);
                mIsInsertionForTesting = true;
                break;

            case SelectionEventType.INSERTION_HANDLE_MOVED:
                mSelectionRect.set(left, top, right, bottom);
                if (!getGestureListenerManager().isScrollInProgress() && isPastePopupShowing()) {
                    showPastePopup();
                } else {
                    destroyPastePopup();
                }
                if (mIsInHandleDragging) {
                    performHapticFeedback();
                }
                break;

            case SelectionEventType.INSERTION_HANDLE_TAPPED:
                if (mWasPastePopupShowingOnInsertionDragStart) {
                    destroyPastePopup();
                } else {
                    showContextMenuAtTouchHandle(mSelectionRect.left, mSelectionRect.bottom);
                }
                mWasPastePopupShowingOnInsertionDragStart = false;
                break;

            case SelectionEventType.INSERTION_HANDLE_CLEARED:
                destroyPastePopup();
                mIsInsertionForTesting = false;
                if (!hasSelection()) mSelectionRect.setEmpty();
                break;

            case SelectionEventType.INSERTION_HANDLE_DRAG_STARTED:
                mWasPastePopupShowingOnInsertionDragStart = isPastePopupShowing();
                destroyPastePopup();
                mIsInHandleDragging = true;
                break;

            case SelectionEventType.INSERTION_HANDLE_DRAG_STOPPED:
                if (mWasPastePopupShowingOnInsertionDragStart) {
                    showContextMenuAtTouchHandle(mSelectionRect.left, mSelectionRect.bottom);
                }
                mWasPastePopupShowingOnInsertionDragStart = false;
                if (mMagnifierAnimator != null) {
                    mMagnifierAnimator.handleDragStopped();
                }
                mIsInHandleDragging = false;
                break;

            default:
                assert false : "Invalid selection event type.";
        }

        if (mSelectionClient != null) {
            final float deviceScale = getDeviceScaleFactor();
            final int xAnchorPix = (int) (mSelectionRect.left * deviceScale);
            final int yAnchorPix = (int) (mSelectionRect.bottom * deviceScale);
            mSelectionClient.onSelectionEvent(eventType, xAnchorPix, yAnchorPix);
        }
    }

    @VisibleForTesting
    /* package */ GestureListenerManagerImpl getGestureListenerManager() {
        return GestureListenerManagerImpl.fromWebContents(mWebContents);
    }

    @VisibleForTesting
    @CalledByNative
    /* package */ void onDragUpdate(@TouchSelectionDraggableType int type, float x, float y) {
        // If this is for longpress drag selector, we can only have mangifier on S and above.
        if (type == TouchSelectionDraggableType.LONGPRESS
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return;
        }

        if (mMagnifierAnimator != null) {
            final float deviceScale = getDeviceScaleFactor();
            x *= deviceScale;
            // The selection coordinates are relative to the content viewport, but we need
            // coordinates relative to the containing View, so adding getContentOffsetYPix().
            y = y * deviceScale + mWebContents.getRenderCoordinates().getContentOffsetYPix();
            mMagnifierAnimator.handleDragStartedOrMoved(x, y);
        }
    }

    @Override
    public void clearSelection() {
        if (mWebContents == null || !isActionModeSupported()) return;
        mWebContents.collapseSelection();
        mClassificationResult = null;
        mHasSelection = false;
    }

    private PopupController getPopupController() {
        if (mPopupController == null) {
            mPopupController = PopupController.fromWebContents(mWebContents);
        }
        return mPopupController;
    }

    @VisibleForTesting
    /* package */ void performHapticFeedback() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && mView != null) {
            mView.performHapticFeedback(HapticFeedbackConstants.TEXT_HANDLE_MOVE);
        }
    }

    /**
     * @return The context used for SelectionPopupController.
     */
    @CalledByNative
    private Context getContext() {
        return mContext;
    }

    @VisibleForTesting
    @CalledByNative
    /* package */ void onSelectionChanged(String text) {
        final boolean unSelected = TextUtils.isEmpty(text) && hasSelection();
        if (unSelected) {
            if (mSmartSelectionEventProcessor != null) {
                mSmartSelectionEventProcessor.onSelectionAction(mLastSelectedText,
                        mLastSelectionOffset, SelectionEvent.ACTION_ABANDON,
                        /* SelectionClient.Result = */ null);
            }
            destroyActionModeAndKeepSelection();
        }
        mLastSelectedText = text;
        if (mSelectionClient != null) {
            mSelectionClient.onSelectionChanged(text);
        }
    }

    /**
     * Sets the client that implements selection augmenting functionality, or null if none exists.
     */
    @Override
    public void setSelectionClient(@Nullable SelectionClient selectionClient) {
        mSelectionClient = selectionClient;
        mSmartSelectionEventProcessor = mSelectionClient == null
                ? null
                : (SmartSelectionEventProcessor) mSelectionClient.getSelectionEventProcessor();

        mClassificationResult = null;

        assert !mHidden;
    }

    /**
     * Sets the handle observer, or null if none exists.
     */
    @VisibleForTesting
    void setMagnifierAnimator(@Nullable MagnifierAnimator magnifierAnimator) {
        mMagnifierAnimator = magnifierAnimator;
    }

    private void initMagnifier() {
        if (sDisableMagnifier || Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return;
        mMagnifierAnimator = new MagnifierAnimator(new MagnifierWrapperImpl(() -> {
            if (sShouldGetReadbackViewFromWindowAndroid) {
                return mWindowAndroid == null ? null : mWindowAndroid.getReadbackView();
            } else {
                return mView;
            }
        }));
    }

    @CalledByNative
    private void onSelectAroundCaretSuccess(int extendedStartAdjust, int extendedEndAdjust,
            int wordStartAdjust, int wordEndAdjust) {
        if (mSelectionClient != null) {
            SelectAroundCaretResult result = new SelectAroundCaretResult(
                    extendedStartAdjust, extendedEndAdjust, wordStartAdjust, wordEndAdjust);
            mSelectionClient.selectAroundCaretAck(result);
        }
    }

    @CalledByNative
    private void onSelectAroundCaretFailure() {
        if (mSelectionClient != null) {
            mSelectionClient.selectAroundCaretAck(null);
        }
    }

    @CalledByNative
    public void hidePopupsAndPreserveSelection() {
        destroyActionModeAndKeepSelection();
        getPopupController().hideAllPopups();
    }

    public void destroyActionModeAndUnselect() {
        mUnselectAllOnDismiss = true;
        finishActionMode();
    }

    public void destroyActionModeAndKeepSelection() {
        mUnselectAllOnDismiss = false;
        finishActionMode();
    }

    public void updateSelectionState(boolean editable, boolean isPassword) {
        if (!editable) destroyPastePopup();
        if (editable != isFocusedNodeEditable() || isPassword != isSelectionPassword()) {
            mEditable = editable;
            mIsPasswordType = isPassword;
            if (isActionModeValid()) mActionMode.invalidate();
        }
    }

    @Override
    public boolean hasSelection() {
        return mHasSelection;
    }

    @Override
    public String getSelectedText() {
        return mLastSelectedText;
    }

    private void setActionMode(ActionMode actionMode) {
        mActionMode = actionMode;
        mIsActionBarShowingSupplier.set(isSelectActionBarShowing());
    }

    private boolean isShareAvailable() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        return PackageManagerUtils.canResolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY);
    }

    // The callback class that delivers the result from a SmartSelectionClient.
    private class SmartSelectionCallback implements SelectionClient.ResultCallback {
        @Override
        public void onClassified(SelectionClient.Result result) {
            // If the selection does not exist any more, discard |result|.
            if (!hasSelection()) {
                mClassificationResult = null;
                return;
            }

            // Do not allow classifier to shorten the selection. If the suggested selection is
            // smaller than the original we throw away classification result and show the menu.
            // TODO(amaralp): This was added to fix the SelectAll problem in
            // http://crbug.com/714106. Once we know the cause of the original selection we can
            // remove this check.
            if (result.startAdjust > 0 || result.endAdjust < 0) {
                mClassificationResult = null;
                showActionModeOrClearOnFailure();
                return;
            }

            // The classificationresult is a property of the selection. Keep it even the action
            // mode has been dismissed.
            mClassificationResult = result;

            // Update the selection range if needed.
            if (!(result.startAdjust == 0 && result.endAdjust == 0)) {
                // This call will cause showSelectionMenu again.
                mWebContents.adjustSelectionByCharacterOffset(
                        result.startAdjust, result.endAdjust, /* showSelectionMenu = */ true);
                return;
            }

            // We won't do expansion here, however, we want to 1) for starting a new logging
            // session, log non selection expansion event to match the behavior of expansion case.
            // 2) log selection handle dragging triggered selection change.
            if (mSmartSelectionEventProcessor != null) {
                mSmartSelectionEventProcessor.onSelectionModified(
                        mLastSelectedText, mLastSelectionOffset, mClassificationResult);
            }

            // Rely on this method to clear |mHidden| and unhide the action mode.
            showActionModeOrClearOnFailure();
        }
    };

    @Override
    public void destroySelectActionMode() {
        finishActionMode();
    }

    @Override
    public boolean isSelectActionBarShowing() {
        return isActionModeValid();
    }

    @Override
    public ObservableSupplier<Boolean> isSelectActionBarShowingSupplier() {
        return mIsActionBarShowingSupplier;
    }

    @Override
    public ActionModeCallbackHelper getActionModeCallbackHelper() {
        return this;
    }

    @Override
    public void setTextClassifier(TextClassifier textClassifier) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        SelectionClient client = getSelectionClient();
        if (client != null) client.setTextClassifier(textClassifier);
    }

    @Override
    public TextClassifier getTextClassifier() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        SelectionClient client = getSelectionClient();
        return client == null ? null : client.getTextClassifier();
    }

    @Override
    public TextClassifier getCustomTextClassifier() {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
        SelectionClient client = getSelectionClient();
        return client == null ? null : client.getCustomTextClassifier();
    }

    @CalledByNative
    private void nativeSelectionPopupControllerDestroyed() {
        mNativeSelectionPopupController = 0;
    }

    @NativeMethods
    interface Natives {
        long init(SelectionPopupControllerImpl caller, WebContents webContents);
        void setTextHandlesTemporarilyHidden(long nativeSelectionPopupController,
                SelectionPopupControllerImpl caller, boolean hidden);
    }
}
