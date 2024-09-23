// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import static org.hamcrest.CoreMatchers.is;

import android.app.Activity;
import android.graphics.Rect;
import android.util.JsonReader;
import android.view.View;

import org.hamcrest.Matchers;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;
import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.WebContents;

import java.io.IOException;
import java.io.StringReader;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Collection of DOM-based utilities. */
@JNINamespace("content")
public class DOMUtils {
    private static final long MEDIA_TIMEOUT_SECONDS = 10L;
    private static final long MEDIA_TIMEOUT_MILLISECONDS = MEDIA_TIMEOUT_SECONDS * 1000;
    private static final String RESULT_OK = "RESULT_OK";
    private static final String RESULT_ELEMENT_NOT_FOUND = "RESULT_ELEMENT_NOT_FOUND";

    /**
     * Plays the media with given {@code id}.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to be played.
     */
    public static void playMedia(final WebContents webContents, final String id)
            throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var media = document.getElementById('" + id + "');");
        sb.append("  if (media) media.play();");
        sb.append("})();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString(), MEDIA_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Pauses the media with given {@code id}
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to be paused.
     */
    public static void pauseMedia(final WebContents webContents, final String id)
            throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var media = document.getElementById('" + id + "');");
        sb.append("  if (media) media.pause();");
        sb.append("})();");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, sb.toString(), MEDIA_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    /**
     * Returns whether the media with given {@code id} is paused.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     * @return whether the media is paused.
     */
    public static boolean isMediaPaused(final WebContents webContents, final String id)
            throws TimeoutException {
        return getNodeField("paused", webContents, id, Boolean.class);
    }

    /**
     * Returns whether the media with given {@code id} has ended.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     * @return whether the media has ended.
     */
    public static boolean isMediaEnded(final WebContents webContents, final String id)
            throws TimeoutException {
        return getNodeField("ended", webContents, id, Boolean.class);
    }

    /**
     * Returns the current time of the media with given {@code id}.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     * @return the current time (in seconds) of the media.
     */
    private static double getCurrentTime(final WebContents webContents, final String id)
            throws TimeoutException {
        return getNodeField("currentTime", webContents, id, Double.class);
    }

    /**
     * Waits until the playback of the media with given {@code id} has started.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     */
    public static void waitForMediaPlay(final WebContents webContents, final String id) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        // Playback can't be reliably detected until current time moves forward.
                        Criteria.checkThat(
                                DOMUtils.isMediaPaused(webContents, id), Matchers.is(false));
                        Criteria.checkThat(
                                DOMUtils.getCurrentTime(webContents, id), Matchers.greaterThan(0d));
                    } catch (TimeoutException e) {
                        // Intentionally do nothing
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                MEDIA_TIMEOUT_MILLISECONDS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits until the playback of the media with given {@code id} has paused before ended.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     */
    public static void waitForMediaPauseBeforeEnd(final WebContents webContents, final String id) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                DOMUtils.isMediaPaused(webContents, id), Matchers.is(true));
                        Criteria.checkThat(
                                DOMUtils.isMediaEnded(webContents, id), Matchers.is(false));
                    } catch (TimeoutException e) {
                        // Intentionally do nothing
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /**
     * Returns whether the document is fullscreen.
     * @param webContents The WebContents to check.
     * @return Whether the document is fullsscreen.
     */
    public static boolean isFullscreen(final WebContents webContents) throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  return [document.webkitIsFullScreen];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
        return readValue(jsonText, Boolean.class);
    }

    /**
     * Makes the document exit fullscreen.
     * @param webContents The WebContents to make fullscreen.
     */
    public static void exitFullscreen(final WebContents webContents) {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  if (document.webkitExitFullscreen) document.webkitExitFullscreen();");
        sb.append("})();");

        JavaScriptUtils.executeJavaScript(webContents, sb.toString());
    }

    private static View getContainerView(final WebContents webContents) {
        return ((WebContentsImpl) webContents).getViewAndroidDelegate().getContainerView();
    }

    private static Activity getActivity(final WebContents webContents) {
        return ContextUtils.activityFromContext(((WebContentsImpl) webContents).getContext());
    }

    /**
     * Returns the rect boundaries for a node by its id.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return The rect boundaries for the node.
     */
    public static Rect getNodeBounds(final WebContents webContents, String nodeId)
            throws TimeoutException {
        String jsCode = "document.getElementById('" + nodeId + "')";
        return getNodeBoundsByJs(webContents, jsCode);
    }

    /**
     * Returns the rect with the document viewport.
     *
     * @param webContents The WebContents in which the node lives.
     * @return The rect for the viewport, which always has a [0,0] top left.
     */
    public static Rect getDocumentViewport(final WebContents webContents) throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append(
                "  return [document.documentElement.clientWidth,"
                        + " document.documentElement.clientHeight];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
        Assert.assertFalse(
                "Failed to retrieve document viewport", jsonText.trim().equalsIgnoreCase("null"));
        int[] wh = readJsonIntArray(jsonText, 2);
        return new Rect(0, 0, wh[0], wh[1]);
    }

    /**
     * Returns the client rect for a node by its id.
     *
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return The client rect for the node.
     */
    public static Rect getNodeClientRect(final WebContents webContents, String nodeId)
            throws TimeoutException {
        String elementGetterJs = "document.getElementById('" + nodeId + "')";

        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = " + elementGetterJs + ";");
        sb.append("  if (!node) return null;");
        sb.append("  var r = node.getBoundingClientRect();");
        sb.append(
                "  return [Math.round(r.left), Math.round(r.top), Math.round(r.right),"
                        + " Math.round(r.bottom)];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());

        Assert.assertFalse(
                "Failed to retrieve client rect for element: " + elementGetterJs,
                jsonText.trim().equalsIgnoreCase("null"));
        int[] r = readJsonIntArray(jsonText, 4);

        return new Rect(r[0], r[1], r[2], r[3]);
    }

    /**
     * Focus a DOM node by its id.
     *
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void focusNode(final WebContents webContents, String nodeId)
            throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (node) node.focus();");
        sb.append("})();");

        JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
    }

    /**
     * Get the id of the currently focused node.
     * @param webContents The WebContents in which the node lives.
     * @return The id of the currently focused node.
     */
    public static String getFocusedNode(WebContents webContents) throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.activeElement;");
        sb.append("  if (!node) return null;");
        sb.append("  return node.id;");
        sb.append("})();");

        String id = JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());

        // String results from JavaScript includes surrounding quotes.  Remove them.
        if (id != null && id.length() >= 2 && id.charAt(0) == '"') {
            id = id.substring(1, id.length() - 1);
        }
        return id;
    }

    /**
     * Click a DOM node by its id, scrolling it into view first.
     * Warning: This method might cause flakiness in the tests
     * See http://crbug.com/1327063
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static boolean clickNode(final WebContents webContents, String nodeId)
            throws TimeoutException {
        return clickNode(webContents, nodeId, /* goThroughRootAndroidView= */ true);
    }

    /**
     * Click a DOM node by its id, scrolling it into view first.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param goThroughRootAndroidView Whether the input should be routed through the Root View for
     *        the CVC.
     */
    public static boolean clickNode(
            final WebContents webContents, String nodeId, boolean goThroughRootAndroidView)
            throws TimeoutException {
        return clickNode(
                webContents, nodeId, goThroughRootAndroidView, /* shouldScrollIntoView= */ true);
    }

    /**
     * Click a DOM node by its id.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param goThroughRootAndroidView Whether the input should be routed through the Root View for
     *        the CVC.
     * @param shouldScrollIntoView Whether to scroll the node into view first.
     */
    public static boolean clickNode(
            final WebContents webContents,
            String nodeId,
            boolean goThroughRootAndroidView,
            boolean shouldScrollIntoView)
            throws TimeoutException {
        if (shouldScrollIntoView) scrollNodeIntoView(webContents, nodeId);
        int[] clickTarget = getClickTargetForNode(webContents, nodeId);
        if (goThroughRootAndroidView) {
            return TouchCommon.singleClickView(
                    getContainerView(webContents), clickTarget[0], clickTarget[1]);
        } else {
            // TODO(mthiesse): It should be sufficient to use getContainerView(webContents) here
            // directly, but content offsets are only updated in the EventForwarder when the
            // CompositorViewHolder intercepts touch events.
            View target =
                    getContainerView(webContents).getRootView().findViewById(android.R.id.content);
            return TouchCommon.singleClickViewThroughTarget(
                    getContainerView(webContents), target, clickTarget[0], clickTarget[1]);
        }
    }

    /**
     * Click a DOM node returned by JS code, scrolling it into view first.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode The JS code to find the node.
     */
    public static void clickNodeByJs(final WebContents webContents, String jsCode)
            throws TimeoutException {
        scrollNodeIntoViewByJs(webContents, jsCode);
        int[] clickTarget = getClickTargetForNodeByJs(webContents, jsCode);
        TouchCommon.singleClickView(getContainerView(webContents), clickTarget[0], clickTarget[1]);
    }

    /**
     * Click a given rect in the page. Does not move the rect into view.
     * @param webContents The WebContents in which the node lives.
     * @param rect The rect to click.
     */
    public static boolean clickRect(final WebContents webContents, Rect rect) {
        int[] clickTarget = getClickTargetForBounds(webContents, rect);
        return TouchCommon.singleClickView(
                getContainerView(webContents), clickTarget[0], clickTarget[1]);
    }

    /**
     * Starts (synchronously) a drag motion on the specified coordinates of a DOM node by its id,
     * scrolling it into view first. Normally followed by dragNodeTo() and dragNodeEnd().
     *
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    public static void dragNodeStart(final WebContents webContents, String nodeId, long downTime)
            throws TimeoutException {
        scrollNodeIntoView(webContents, nodeId);
        String jsCode = "document.getElementById('" + nodeId + "')";
        int[] fromTarget = getClickTargetForNodeByJs(webContents, jsCode);
        TouchCommon.dragStart(getActivity(webContents), fromTarget[0], fromTarget[1], downTime);
    }

    /**
     * Drags / moves (synchronously) to the specified coordinates of a DOM node by its id. Normally
     * preceded by dragNodeStart() and followed by dragNodeEnd()
     *
     * @param webContents The WebContents in which the node lives.
     * @param fromNodeId The id of the node's coordinates of the initial touch.
     * @param toNodeId The id of the node's coordinates of the drag destination.
     * @param stepCount How many move steps to include in the drag.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    public static void dragNodeTo(
            final WebContents webContents,
            String fromNodeId,
            String toNodeId,
            int stepCount,
            long downTime)
            throws TimeoutException {
        int[] fromTarget =
                getClickTargetForNodeByJs(
                        webContents, "document.getElementById('" + fromNodeId + "')");
        int[] toTarget =
                getClickTargetForNodeByJs(
                        webContents, "document.getElementById('" + toNodeId + "')");
        TouchCommon.dragTo(
                getActivity(webContents),
                fromTarget[0],
                fromTarget[1],
                toTarget[0],
                toTarget[1],
                stepCount,
                downTime);
    }

    /**
     * Finishes (synchronously) a drag / move at the specified coordinate of a DOM node by its id,
     * scrolling it into view first. Normally preceded by dragNodeStart() and dragNodeTo().
     *
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param downTime When the drag was started, in millis since the epoch.
     */
    public static void dragNodeEnd(final WebContents webContents, String nodeId, long downTime)
            throws TimeoutException {
        scrollNodeIntoView(webContents, nodeId);
        String jsCode = "document.getElementById('" + nodeId + "')";
        int[] endTarget = getClickTargetForNodeByJs(webContents, jsCode);
        TouchCommon.dragEnd(getActivity(webContents), endTarget[0], endTarget[1], downTime);
    }

    /**
     * Long-press a DOM node by its id, scrolling it into view first and without release.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param downTime When the Long-press was started, in millis since the epoch.
     */
    public static void longPressNodeWithoutUp(
            final WebContents webContents, String nodeId, long downTime) throws TimeoutException {
        scrollNodeIntoView(webContents, nodeId);
        String jsCode = "document.getElementById('" + nodeId + "')";
        longPressNodeWithoutUpByJs(webContents, jsCode, downTime);
    }

    /**
     * Long-press a DOM node by its id, without release.
     * <p>Note that content view should be located in the current position for a foreseeable
     * amount of time because this involves sleep to simulate touch to long press transition.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode js code that returns an element.
     * @param downTime When the Long-press was started, in millis since the epoch.
     */
    public static void longPressNodeWithoutUpByJs(
            final WebContents webContents, String jsCode, long downTime) throws TimeoutException {
        int[] clickTarget = getClickTargetForNodeByJs(webContents, jsCode);
        TouchCommon.longPressViewWithoutUp(
                getContainerView(webContents), clickTarget[0], clickTarget[1], downTime);
    }

    /**
     * Long-press a DOM node by its id, scrolling it into view first.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void longPressNode(final WebContents webContents, String nodeId)
            throws TimeoutException {
        scrollNodeIntoView(webContents, nodeId);
        String jsCode = "document.getElementById('" + nodeId + "')";
        longPressNodeByJs(webContents, jsCode);
    }

    /**
     * Long-press a DOM node by its id.
     * <p>Note that content view should be located in the current position for a foreseeable
     * amount of time because this involves sleep to simulate touch to long press transition.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode js code that returns an element.
     */
    public static void longPressNodeByJs(final WebContents webContents, String jsCode)
            throws TimeoutException {
        int[] clickTarget = getClickTargetForNodeByJs(webContents, jsCode);
        TouchCommon.longPressView(getContainerView(webContents), clickTarget[0], clickTarget[1]);
    }

    /**
     * Scrolls the view to ensure that the required DOM node is visible.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void scrollNodeIntoView(WebContents webContents, String nodeId)
            throws TimeoutException {
        scrollNodeIntoViewByJs(webContents, "document.getElementById('" + nodeId + "')");
    }

    /**
     * Scrolls the view to ensure that the required DOM node is visible.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode The JS code to find the node.
     */
    public static void scrollNodeIntoViewByJs(WebContents webContents, String jsCode)
            throws TimeoutException {
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents, jsCode + ".scrollIntoView()");
    }

    /**
     * Returns the text contents of a given node.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the text contents of the node.
     */
    public static String getNodeContents(WebContents webContents, String nodeId)
            throws TimeoutException {
        return getNodeField("textContent", webContents, nodeId, String.class);
    }

    /**
     * Returns the value of a given node.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the value of the node.
     */
    public static String getNodeValue(final WebContents webContents, String nodeId)
            throws TimeoutException {
        return getNodeField("value", webContents, nodeId, String.class);
    }

    /**
     * Returns the string value of a field of a given node.
     * @param fieldName The field to return the value from.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the value of the field.
     */
    public static String getNodeField(
            String fieldName, final WebContents webContents, String nodeId)
            throws TimeoutException {
        return getNodeField(fieldName, webContents, nodeId, String.class);
    }

    /**
     * Wait until a given node has non-zero bounds.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void waitForNonZeroNodeBounds(
            final WebContents webContents, final String nodeId) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                DOMUtils.getNodeBounds(webContents, nodeId).isEmpty(),
                                Matchers.is(false));
                    } catch (TimeoutException e) {
                        // Intentionally do nothing
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }

    /**
     * Returns the value of a given field of type {@code valueType} as a {@code T}.
     * @param fieldName The field to return the value from.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param valueType The type of the value to read.
     * @return the field's value.
     */
    public static <T> T getNodeField(
            String fieldName, final WebContents webContents, String nodeId, Class<T> valueType)
            throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  return [ node." + fieldName + " ];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
        Assert.assertFalse(
                "Failed to retrieve contents for " + nodeId,
                jsonText.trim().equalsIgnoreCase("null"));
        return readValue(jsonText, valueType);
    }

    /**
     * Returns the value of a given attribute of type {@code valueType} as a {@code T} or null.
     * @param attributeName The attribute to return the value from.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param valueType The type of the value to read.
     * @return the attributes' value or null if there is no attribute with such attributeName.
     */
    public static <T> T getNodeAttribute(
            String attributeName, final WebContents webContents, String nodeId, Class<T> valueType)
            throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  var nodeAttr = node.getAttribute('" + attributeName + "');");
        sb.append("  if (!nodeAttr) return null;");
        sb.append("  return [ nodeAttr ];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
        if (jsonText.trim().equalsIgnoreCase("null")) {
            return null;
        }
        return readValue(jsonText, valueType);
    }

    /**
     * Click a DOM node by its id using a js MouseEvent with a fake gesture.
     * This function is more reliable than {@link #clickNode(WebContents, String)},
     * but it doesn't simulate a screen touch.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void clickNodeWithJavaScript(WebContents webContents, String nodeId) {
        WebContentsUtils.evaluateJavaScriptWithUserGesture(
                webContents, createScriptToClickNode(nodeId), null);
    }

    /**
     * Returns the next value of type {@code valueType} as a {@code T}.
     * @param jsonText The unparsed json text.
     * @param valueType The type of the value to read.
     * @return the read value.
     */
    private static <T> T readValue(String jsonText, Class<T> valueType) {
        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        T value = null;
        try {
            jsonReader.beginArray();
            if (jsonReader.hasNext()) value = readValue(jsonReader, valueType);
            jsonReader.endArray();
            Assert.assertNotNull("Invalid contents returned.", value);

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to evaluate JavaScript: " + jsonText + "\n" + exception);
        }
        return value;
    }

    /**
     * Returns the next value of type {@code valueType} as a {@code T}.
     * @param jsonReader JsonReader instance to be used.
     * @param valueType The type of the value to read.
     * @throws IllegalArgumentException If the {@code valueType} isn't known.
     * @return the read value.
     */
    @SuppressWarnings("unchecked")
    private static <T> T readValue(JsonReader jsonReader, Class<T> valueType) throws IOException {
        if (valueType.equals(String.class)) return ((T) jsonReader.nextString());
        if (valueType.equals(Boolean.class)) return ((T) ((Boolean) jsonReader.nextBoolean()));
        if (valueType.equals(Integer.class)) return ((T) ((Integer) jsonReader.nextInt()));
        if (valueType.equals(Long.class)) return ((T) ((Long) jsonReader.nextLong()));
        if (valueType.equals(Double.class)) return ((T) ((Double) jsonReader.nextDouble()));

        throw new IllegalArgumentException("Cannot read values of type " + valueType);
    }

    /**
     * Returns click target for a given DOM node.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @return the click target of the node in the form of a [ x, y ] array.
     */
    private static int[] getClickTargetForNode(WebContents webContents, String nodeId)
            throws TimeoutException {
        String jsCode = "document.getElementById('" + nodeId + "')";
        return getClickTargetForNodeByJs(webContents, jsCode);
    }

    /**
     * Returns click target for a given DOM node.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode The javascript to get the node.
     * @return the click target of the node in the form of a [ x, y ] array.
     */
    private static int[] getClickTargetForNodeByJs(WebContents webContents, String jsCode)
            throws TimeoutException {
        Rect bounds = getNodeBoundsByJs(webContents, jsCode);
        Assert.assertNotNull(
                "Failed to get DOM element bounds of element='" + jsCode + "'.", bounds);

        return getClickTargetForBounds(webContents, bounds);
    }

    /**
     * Returns click target for the DOM node specified by the rect boundaries.
     * @param webContents The WebContents in which the node lives.
     * @param bounds The rect boundaries of a DOM node.
     * @return the click target of the node in the form of a [ x, y ] array.
     */
    private static int[] getClickTargetForBounds(WebContents webContents, Rect bounds) {
        // TODO(nburris): This converts from CSS pixels to physical pixels, but
        // does not account for visual viewport offset.
        RenderCoordinatesImpl coord = ((WebContentsImpl) webContents).getRenderCoordinates();
        int clickX = (int) coord.fromLocalCssToPix(bounds.exactCenterX());
        int clickY =
                (int) coord.fromLocalCssToPix(bounds.exactCenterY())
                        + getMaybeTopControlsHeight(webContents);
        return new int[] {clickX, clickY};
    }

    private static int getMaybeTopControlsHeight(final WebContents webContents) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> DOMUtilsJni.get().getTopControlsShrinkBlinkHeight(webContents));
    }

    /**
     * Returns the rect boundaries for a node by the javascript to get the node.
     * @param webContents The WebContents in which the node lives.
     * @param jsCode The javascript to get the node.
     * @return The rect boundaries for the node.
     */
    private static Rect getNodeBoundsByJs(final WebContents webContents, String jsCode)
            throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = " + jsCode + ";");
        sb.append("  if (!node) return null;");
        sb.append("  var width = Math.round(node.offsetWidth);");
        sb.append("  var height = Math.round(node.offsetHeight);");
        sb.append("  var x = -window.scrollX;");
        sb.append("  var y = -window.scrollY;");
        sb.append("  do {");
        sb.append("    x += node.offsetLeft;");
        sb.append("    y += node.offsetTop;");
        sb.append("  } while (node = node.offsetParent);");
        sb.append("  return [ Math.round(x), Math.round(y), width, height ];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());

        Assert.assertFalse(
                "Failed to retrieve bounds for element: " + jsCode,
                jsonText.trim().equalsIgnoreCase("null"));

        int[] bounds = readJsonIntArray(jsonText, 4);
        return new Rect(bounds[0], bounds[1], bounds[0] + bounds[2], bounds[1] + bounds[3]);
    }

    private static int[] readJsonIntArray(String jsonText, int size) {
        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        int[] result = new int[size];
        int i = 0;
        try {
            jsonReader.beginArray();
            while (jsonReader.hasNext()) {
                if (i >= size) {
                    Assert.fail("Json array was larger than size " + size + ": " + jsonText);
                }
                result[i++] = jsonReader.nextInt();
            }
            jsonReader.endArray();
            Assert.assertEquals("Json array was smaller than size " + size, size, i);

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to read json array: " + jsonText + "\n" + exception);
        }
        return result;
    }

    private static String createScriptToClickNode(String nodeId) {
        String script = "document.getElementById('" + nodeId + "').click();";
        return script;
    }

    /**
     * Prints the text into the text field node simulating the keyboard input. The node needs to be
     * focused at first to bring up the keyboard.
     *
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the text input node.
     * @param input The text to be entered into the text field.
     */
    public static void enterInputIntoTextField(WebContents webContents, String nodeId, String input)
            throws TimeoutException {
        Assert.assertTrue(
                "Input should be a non-empty string", input != null && input.length() > 0);
        ImeAdapter imeAdapter = WebContentsUtils.getImeAdapter(webContents);
        TestInputMethodManagerWrapper inputMethodManagerWrapper =
                TestInputMethodManagerWrapper.create(imeAdapter);
        imeAdapter.setInputMethodManagerWrapper(inputMethodManagerWrapper);
        // Click the text field node, so that it would get focus.
        DOMUtils.clickNode(webContents, nodeId);
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(DOMUtils.getFocusedNode(webContents), is(nodeId));
                    } catch (TimeoutException e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });

        // Wait for the text field to get focused and the virtual keyboard to be activated.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            inputMethodManagerWrapper.isActive(
                                    DOMUtils.getContainerView(webContents)),
                            is(true));
                });

        // Enter the text.
        imeAdapter.setComposingTextForTest(input, 1);
        // Wait for the input to finish. After finishing the input, it will update the selection to
        // move the cursor to the right position. This indicated that the input has finished.
        waitForTextFieldValue(webContents, nodeId, input);
    }

    private static void waitForTextFieldValue(
            WebContents webContents, String textFieldId, String value) throws TimeoutException {
        StringBuilder func = new StringBuilder();
        func.append("function valueCheck() {");
        func.append("  var element = document.getElementById('" + textFieldId + "');");
        func.append("  return element && element.value == '" + value + "';");
        func.append("}");

        func.append("(async function() {");
        func.append("var res = await new Promise(resolve => {");
        func.append("  if (valueCheck()) {");
        func.append("    return resolve('" + RESULT_OK + "');");

        func.append("  } else {");
        func.append("    var element = document.getElementById('" + textFieldId + "');");
        func.append("    if (!element)");
        func.append("      return resolve('" + RESULT_ELEMENT_NOT_FOUND + "');");

        func.append("    element.oninput = function() {");
        func.append("      if (valueCheck()) {");
        func.append("        element.oninput = undefined;");
        func.append("        return resolve('" + RESULT_OK + "');");
        func.append("      }");
        func.append("    };");
        func.append("  }");
        func.append("});");
        func.append("window.domAutomationController.send([res]);");
        func.append("})();");

        String jsonText =
                JavaScriptUtils.runJavascriptWithAsyncResult(webContents, func.toString());
        Assert.assertFalse(
                "Failed to verify input for field " + textFieldId,
                jsonText.trim().equalsIgnoreCase("null"));
        String result = readValue(jsonText, String.class);
        if (RESULT_ELEMENT_NOT_FOUND.equals(result)) {
            Assert.fail(
                    "Expected to find element with id " + textFieldId + ", but didn't find any.");
        }
        if (!RESULT_OK.equals(result)) {
            Assert.fail(
                    "Actual value of the field "
                            + textFieldId
                            + " is different from the expected value "
                            + value
                            + ".");
        }
    }

    @NativeMethods
    interface Natives {
        int getTopControlsShrinkBlinkHeight(WebContents webContents);
    }
}
