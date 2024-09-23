// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Handler;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import androidx.annotation.RequiresApi;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.content.browser.ViewEventSinkImpl;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.test.RenderFrameHostTestExt;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper;
import org.chromium.content_public.browser.test.util.TestInputMethodManagerWrapper.InputConnectionProvider;
import org.chromium.content_shell_apk.ContentShellActivityTestRule;
import org.chromium.ui.base.ime.TextInputType;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** Integration tests for text input for Android L (or above) features. */
class ImeActivityTestRule extends ContentShellActivityTestRule {
    private ChromiumBaseInputConnection mConnection;
    private TestInputConnectionFactory mConnectionFactory;
    private ImeAdapterImpl mImeAdapter;

    static final String INPUT_FORM_HTML = "content/test/data/android/input/input_forms.html";
    static final String PASSWORD_FORM_HTML = "content/test/data/android/input/password_form.html";
    static final String INPUT_MODE_HTML = "content/test/data/android/input/input_mode.html";
    static final String INPUT_ACTION_HTML = "content/test/data/android/input/input_action.html";
    static final String INPUT_VK_API_HTML =
            "content/test/data/android/input/virtual_keyboard_api.html";

    private SelectionPopupControllerImpl mSelectionPopupController;
    private TestCallbackHelperContainer mCallbackContainer;
    private TestInputMethodManagerWrapper mInputMethodManagerWrapper;

    public void setUpForUrl(String url) throws Exception {
        launchContentShellWithUrlSync(url);
        mSelectionPopupController = getSelectionPopupController();

        final ImeAdapter imeAdapter = getImeAdapter();
        InputConnectionProvider provider =
                TestInputMethodManagerWrapper.defaultInputConnectionProvider(imeAdapter);
        mInputMethodManagerWrapper =
                new TestInputMethodManagerWrapper(provider) {
                    private boolean mExpectsSelectionOutsideComposition;

                    @Override
                    public void expectsSelectionOutsideComposition() {
                        mExpectsSelectionOutsideComposition = true;
                    }

                    @Override
                    public void onUpdateSelection(
                            Range oldSel, Range oldComp, Range newSel, Range newComp) {
                        // We expect that selection will be outside composition in some cases.
                        // Keyboard app will not finish composition in this case.
                        if (mExpectsSelectionOutsideComposition) {
                            mExpectsSelectionOutsideComposition = false;
                            return;
                        }
                        if (oldComp == null
                                || oldComp.start() == oldComp.end()
                                || newComp.start() == newComp.end()) {
                            return;
                        }
                        // This emulates keyboard app's behavior that finishes composition when
                        // selection is outside composition.
                        if (!newSel.intersects(newComp)) {
                            try {
                                finishComposingText();
                            } catch (Exception e) {
                                e.printStackTrace();
                                Assert.fail();
                            }
                        }
                    }
                };
        getImeAdapter().setInputMethodManagerWrapper(mInputMethodManagerWrapper);
        Assert.assertEquals(0, mInputMethodManagerWrapper.getShowSoftInputCounter());
        mConnectionFactory =
                new TestInputConnectionFactory(getImeAdapter().getInputConnectionFactoryForTest());
        getImeAdapter().setInputConnectionFactory(mConnectionFactory);

        WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        mCallbackContainer = new TestCallbackHelperContainer(webContents);
        DOMUtils.waitForNonZeroNodeBounds(webContents, "input_text");
        boolean result = DOMUtils.clickNode(webContents, "input_text");

        Assert.assertEquals("Failed to dispatch touch event.", true, result);
        assertWaitForKeyboardStatus(true);

        mConnection = getInputConnection();
        mImeAdapter = getImeAdapter();

        waitForKeyboardStates(1, 0, 1, new Integer[] {TextInputType.TEXT});
        Assert.assertEquals(0, mConnectionFactory.getOutAttrs().initialSelStart);
        Assert.assertEquals(0, mConnectionFactory.getOutAttrs().initialSelEnd);

        waitForEventLogState("selectionchange");
        clearEventLogs();

        waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        resetAllStates();
    }

    TestCallbackHelperContainer getTestCallBackHelperContainer() {
        return mCallbackContainer;
    }

    ChromiumBaseInputConnection getConnection() {
        return mConnection;
    }

    TestInputMethodManagerWrapper getInputMethodManagerWrapper() {
        return mInputMethodManagerWrapper;
    }

    TestInputConnectionFactory getConnectionFactory() {
        return mConnectionFactory;
    }

    void fullyLoadUrl(final String url) throws Exception {
        CallbackHelper done = mCallbackContainer.getOnFirstVisuallyNonEmptyPaintHelper();
        int currentCallCount = done.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getActivity().getActiveShell().loadUrl(url);
                });
        waitForActiveShellToBeDoneLoading();
        done.waitForCallback(currentCallCount);
    }

    void clearEventLogs() throws Exception {
        final String code = "clearEventLogs()";
        JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), code);
    }

    void waitForEventLogs(String expectedLogs) throws Exception {
        final String code = "getEventLogs()";
        final String sanitizedExpectedLogs = "\"" + expectedLogs + "\"";
        Assert.assertEquals(
                sanitizedExpectedLogs,
                JavaScriptUtils.executeJavaScriptAndWaitForResult(getWebContents(), code));
    }

    void waitForEventLogState(String expectedLogs) {
        final String code = "getEventLogs()";
        final String sanitizedExpectedLogs = "\"" + expectedLogs + "\"";
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        getWebContents(), code),
                                Matchers.is(sanitizedExpectedLogs));
                    } catch (TimeoutException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                });
    }

    void waitForFocusedElement(String id) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                DOMUtils.getFocusedNode(getWebContents()), Matchers.is(id));
                    } catch (TimeoutException ex) {
                        throw new CriteriaNotSatisfiedException(ex);
                    }
                });
    }

    void assertTextsAroundCursor(CharSequence before, CharSequence selected, CharSequence after)
            throws Exception {
        Assert.assertEquals(before, getTextBeforeCursor(100, 0));
        Assert.assertEquals(selected, getSelectedText(0));
        Assert.assertEquals(after, getTextAfterCursor(100, 0));
    }

    void waitForKeyboardStates(int show, int hide, int restart, Integer[] textInputTypeHistory) {
        final String expected =
                stringifyKeyboardStates(show, hide, restart, textInputTypeHistory, null, null);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(getKeyboardStates(false, false), Matchers.is(expected));
                });
    }

    void waitForKeyboardStates(
            int show,
            int hide,
            int restart,
            Integer[] textInputTypeHistory,
            Integer[] textInputModeHistory) {
        final String expected =
                stringifyKeyboardStates(
                        show, hide, restart, textInputTypeHistory, textInputModeHistory, null);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(getKeyboardStates(true, false), Matchers.is(expected));
                });
    }

    void waitForKeyboardInputActionStates(
            int show,
            int hide,
            int restart,
            Integer[] textInputTypeHistory,
            Integer[] textInputActionHistory) {
        final String expected =
                stringifyKeyboardStates(
                        show, hide, restart, textInputTypeHistory, null, textInputActionHistory);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(getKeyboardStates(false, true), Matchers.is(expected));
                });
    }

    void resetAllStates() {
        mInputMethodManagerWrapper.reset();
        mConnectionFactory.resetAllStates();
    }

    String getKeyboardStates(boolean includeInputMode, boolean includeInputAction) {
        int showCount = mInputMethodManagerWrapper.getShowSoftInputCounter();
        int hideCount = mInputMethodManagerWrapper.getHideSoftInputCounter();
        int restartCount = mInputMethodManagerWrapper.getRestartInputCounter();
        Integer[] textInputTypeHistory = mConnectionFactory.getTextInputTypeHistory();
        Integer[] textInputModeHistory = null;
        Integer[] textInputActionHistory = null;
        if (includeInputMode) textInputModeHistory = mConnectionFactory.getTextInputModeHistory();
        if (includeInputAction) {
            textInputActionHistory = mConnectionFactory.getTextInputActionHistory();
        }
        return stringifyKeyboardStates(
                showCount,
                hideCount,
                restartCount,
                textInputTypeHistory,
                textInputModeHistory,
                textInputActionHistory);
    }

    String stringifyKeyboardStates(
            int show,
            int hide,
            int restart,
            Integer[] inputTypeHistory,
            Integer[] inputModeHistory,
            Integer[] inputActionHistory) {
        return "show count: "
                + show
                + ", hide count: "
                + hide
                + ", restart count: "
                + restart
                + ", input type history: "
                + Arrays.deepToString(inputTypeHistory)
                + ", input mode history: "
                + Arrays.deepToString(inputModeHistory)
                + ", input action history: "
                + Arrays.deepToString(inputActionHistory);
    }

    String[] getLastTextHistory() {
        return mConnectionFactory.getTextInputLastTextHistory();
    }

    void waitForEditorAction(final int expectedAction) {
        CriteriaHelper.pollUiThread(
                () -> {
                    EditorInfo editorInfo = mConnectionFactory.getOutAttrs();
                    int actualAction =
                            editorInfo.actionId != 0
                                    ? editorInfo.actionId
                                    : editorInfo.imeOptions & EditorInfo.IME_MASK_ACTION;
                    Criteria.checkThat(actualAction, Matchers.is(expectedAction));
                });
    }

    void performEditorAction(final int action) {
        mConnection.performEditorAction(action);
    }

    void performGo(TestCallbackHelperContainer testCallbackHelperContainer) throws Throwable {
        final InputConnection inputConnection = mConnection;
        final Callable<Void> callable =
                new Callable<Void>() {
                    @Override
                    public Void call() {
                        inputConnection.performEditorAction(EditorInfo.IME_ACTION_GO);
                        return null;
                    }
                };

        handleBlockingCallbackAction(
                testCallbackHelperContainer.getOnPageFinishedHelper(),
                new Runnable() {
                    @Override
                    public void run() {
                        try {
                            runBlockingOnImeThread(callable);
                        } catch (Exception e) {
                            e.printStackTrace();
                            Assert.fail();
                        }
                    }
                });
    }

    void assertWaitForKeyboardStatus(final boolean show) {
        CriteriaHelper.pollUiThread(
                () -> {
                    if (show) {
                        Criteria.checkThat(getInputConnection(), Matchers.notNullValue());
                    }
                    Criteria.checkThat(
                            mInputMethodManagerWrapper.isShowWithoutHideOutstanding(),
                            Matchers.is(show));
                });
    }

    void assertWaitForSelectActionBarStatus(final boolean show) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            mSelectionPopupController.isSelectActionBarShowing(),
                            Matchers.is(show));
                });
    }

    void verifyNoUpdateSelection() {
        final List<Pair<Range, Range>> states = mInputMethodManagerWrapper.getUpdateSelectionList();
        Assert.assertEquals(0, states.size());
    }

    void waitAndVerifyUpdateSelection(
            final int index,
            final int selectionStart,
            final int selectionEnd,
            final int compositionStart,
            final int compositionEnd) {
        final List<Pair<Range, Range>> states = mInputMethodManagerWrapper.getUpdateSelectionList();
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(states.size(), Matchers.greaterThan(index)));
        Pair<Range, Range> selection = states.get(index);
        Assert.assertEquals("Mismatched selection start", selectionStart, selection.first.start());
        Assert.assertEquals("Mismatched selection end", selectionEnd, selection.first.end());
        Assert.assertEquals(
                "Mismatched composition start", compositionStart, selection.second.start());
        Assert.assertEquals("Mismatched composition end", compositionEnd, selection.second.end());
    }

    void resetUpdateSelectionList() {
        mInputMethodManagerWrapper.getUpdateSelectionList().clear();
    }

    void assertClipboardContents(final Activity activity, final String expectedContents) {
        CriteriaHelper.pollUiThread(
                () -> {
                    ClipboardManager clipboardManager =
                            (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
                    ClipData clip = clipboardManager.getPrimaryClip();
                    Criteria.checkThat(clip, Matchers.notNullValue());
                    Criteria.checkThat(clip.getItemCount(), Matchers.is(1));
                    Criteria.checkThat(clip.getItemAt(0).getText(), Matchers.is(expectedContents));
                });
    }

    ChromiumBaseInputConnection getInputConnection() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> (ChromiumBaseInputConnection) getImeAdapter().getInputConnectionForTest());
    }

    void restartInput() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImeAdapter.restartInput();
                });
    }

    // After calling this method, we should call assertClipboardContents() to wait for the clipboard
    // to get updated. See cubug.com/621046
    void copy() {
        final WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.copy();
                });
    }

    void cut() {
        final WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.cut();
                });
    }

    void notifyVirtualKeyboardOverlayRect(int x, int y, int width, int height) {
        final WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        RenderFrameHostTestExt rfh =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> new RenderFrameHostTestExt(webContents.getMainFrame()));
        Assert.assertTrue("Did not get a focused frame", rfh != null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    rfh.notifyVirtualKeyboardOverlayRect(x, y, width, height);
                });
    }

    void setClip(final CharSequence text) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final ClipboardManager clipboardManager =
                            (ClipboardManager)
                                    getActivity().getSystemService(Context.CLIPBOARD_SERVICE);
                    clipboardManager.setPrimaryClip(ClipData.newPlainText(null, text));
                });
    }

    void paste() {
        final WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.paste();
                });
    }

    void selectAll() {
        final WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.selectAll();
                });
    }

    void collapseSelection() {
        final WebContentsImpl webContents = (WebContentsImpl) getWebContents();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    webContents.collapseSelection();
                });
    }

    /**
     * Run the {@Callable} on IME thread (or UI thread if not applicable).
     * @param c The callable
     * @return The result from running the callable.
     */
    <T> T runBlockingOnImeThread(Callable<T> c) throws Exception {
        return ImeTestUtils.runBlockingOnHandler(mConnectionFactory.getHandler(), c);
    }

    boolean beginBatchEdit() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.beginBatchEdit();
                    }
                });
    }

    boolean endBatchEdit() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.endBatchEdit();
                    }
                });
    }

    boolean commitText(final CharSequence text, final int newCursorPosition) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.commitText(text, newCursorPosition);
                    }
                });
    }

    boolean setSelection(final int start, final int end) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.setSelection(start, end);
                    }
                });
    }

    boolean setComposingRegion(final int start, final int end) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.setComposingRegion(start, end);
                    }
                });
    }

    protected boolean setComposingText(final CharSequence text, final int newCursorPosition)
            throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.setComposingText(text, newCursorPosition);
                    }
                });
    }

    boolean finishComposingText() throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.finishComposingText();
                    }
                });
    }

    boolean deleteSurroundingText(final int before, final int after) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.deleteSurroundingText(before, after);
                    }
                });
    }

    // Note that deleteSurroundingTextInCodePoints() was introduced in Android N (Api level 24), but
    // the Android repository used in Chrome is behind that (level 23). So this function can't be
    // called by keyboard apps currently.
    @RequiresApi(24)
    boolean deleteSurroundingTextInCodePoints(final int before, final int after) throws Exception {
        final ThreadedInputConnection connection = (ThreadedInputConnection) mConnection;
        return runBlockingOnImeThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        return connection.deleteSurroundingTextInCodePoints(before, after);
                    }
                });
    }

    CharSequence getTextBeforeCursor(final int length, final int flags) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return connection.getTextBeforeCursor(length, flags);
                    }
                });
    }

    CharSequence getSelectedText(final int flags) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return connection.getSelectedText(flags);
                    }
                });
    }

    CharSequence getTextAfterCursor(final int length, final int flags) throws Exception {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<CharSequence>() {
                    @Override
                    public CharSequence call() {
                        return connection.getTextAfterCursor(length, flags);
                    }
                });
    }

    int getCursorCapsMode(final int reqModes) throws Throwable {
        final ChromiumBaseInputConnection connection = mConnection;
        return runBlockingOnImeThread(
                new Callable<Integer>() {
                    @Override
                    public Integer call() {
                        return connection.getCursorCapsMode(reqModes);
                    }
                });
    }

    void dispatchKeyEvent(final KeyEvent event) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mImeAdapter.dispatchKeyEvent(event);
                });
    }

    void attachPhysicalKeyboard() {
        Configuration hardKeyboardConfig =
                new Configuration(getActivity().getResources().getConfiguration());
        hardKeyboardConfig.keyboard = Configuration.KEYBOARD_QWERTY;
        hardKeyboardConfig.keyboardHidden = Configuration.KEYBOARDHIDDEN_YES;
        hardKeyboardConfig.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_NO;
        onConfigurationChanged(hardKeyboardConfig);
    }

    void detachPhysicalKeyboard() {
        Configuration softKeyboardConfig =
                new Configuration(getActivity().getResources().getConfiguration());
        softKeyboardConfig.keyboard = Configuration.KEYBOARD_NOKEYS;
        softKeyboardConfig.keyboardHidden = Configuration.KEYBOARDHIDDEN_NO;
        softKeyboardConfig.hardKeyboardHidden = Configuration.HARDKEYBOARDHIDDEN_YES;
        onConfigurationChanged(softKeyboardConfig);
    }

    private void onConfigurationChanged(final Configuration config) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ViewEventSinkImpl.from(getWebContents()).onConfigurationChanged(config);
                });
    }

    /**
     * Focus element, wait for a single state update, reset state update list.
     * @param id ID of the element to focus.
     */
    void focusElementAndWaitForStateUpdate(String id) throws TimeoutException {
        resetAllStates();
        focusElement(id);
        waitAndVerifyUpdateSelection(0, 0, 0, -1, -1);
        resetAllStates();
    }

    void focusElement(final String id) throws TimeoutException {
        focusElement(id, true);
    }

    void focusElement(final String id, boolean shouldShowKeyboard) throws TimeoutException {
        DOMUtils.focusNode(getWebContents(), id);
        assertWaitForKeyboardStatus(shouldShowKeyboard);
        waitForFocusedElement(id);
        // When we focus another element, the connection may be recreated.
        mConnection = getInputConnection();
    }

    static class TestInputConnectionFactory implements ChromiumBaseInputConnection.Factory {
        private final ChromiumBaseInputConnection.Factory mFactory;

        private final List<Integer> mTextInputTypeList = new ArrayList<>();
        private final List<Integer> mTextInputModeList = new ArrayList<>();
        private final List<Integer> mTextInputActionList = new ArrayList<>();
        private final List<String> mTextInputLastTextList = new ArrayList<>();
        private EditorInfo mOutAttrs;

        public TestInputConnectionFactory(ChromiumBaseInputConnection.Factory factory) {
            mFactory = factory;
        }

        @Override
        public ChromiumBaseInputConnection initializeAndGet(
                View view,
                ImeAdapterImpl imeAdapter,
                int inputType,
                int inputFlags,
                int inputMode,
                int inputAction,
                int selectionStart,
                int selectionEnd,
                String lastText,
                EditorInfo outAttrs) {
            mTextInputTypeList.add(inputType);
            mTextInputModeList.add(inputMode);
            mTextInputActionList.add(inputAction);
            mTextInputLastTextList.add(lastText);
            mOutAttrs = outAttrs;
            return mFactory.initializeAndGet(
                    view,
                    imeAdapter,
                    inputType,
                    inputFlags,
                    inputMode,
                    inputAction,
                    selectionStart,
                    selectionEnd,
                    lastText,
                    outAttrs);
        }

        @Override
        public Handler getHandler() {
            return mFactory.getHandler();
        }

        public Integer[] getTextInputTypeHistory() {
            Integer[] result = new Integer[mTextInputTypeList.size()];
            mTextInputTypeList.toArray(result);
            return result;
        }

        public void resetAllStates() {
            mTextInputTypeList.clear();
            mTextInputModeList.clear();
            mTextInputActionList.clear();
            mTextInputLastTextList.clear();
        }

        public Integer[] getTextInputModeHistory() {
            Integer[] result = new Integer[mTextInputModeList.size()];
            mTextInputModeList.toArray(result);
            return result;
        }

        public Integer[] getTextInputActionHistory() {
            Integer[] result = new Integer[mTextInputActionList.size()];
            mTextInputActionList.toArray(result);
            return result;
        }

        public String[] getTextInputLastTextHistory() {
            String[] result = new String[mTextInputLastTextList.size()];
            mTextInputLastTextList.toArray(result);
            return result;
        }

        public EditorInfo getOutAttrs() {
            return mOutAttrs;
        }

        @Override
        public void onWindowFocusChanged(boolean gainFocus) {
            mFactory.onWindowFocusChanged(gainFocus);
        }

        @Override
        public void onViewFocusChanged(boolean gainFocus) {
            mFactory.onViewFocusChanged(gainFocus);
        }

        @Override
        public void onViewAttachedToWindow() {
            mFactory.onViewAttachedToWindow();
        }

        @Override
        public void onViewDetachedFromWindow() {
            mFactory.onViewDetachedFromWindow();
        }

        @Override
        public void setTriggerDelayedOnCreateInputConnection(boolean trigger) {
            mFactory.setTriggerDelayedOnCreateInputConnection(trigger);
        }
    }
}
