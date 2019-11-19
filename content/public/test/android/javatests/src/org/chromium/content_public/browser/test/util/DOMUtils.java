// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import android.app.Activity;
import android.graphics.Rect;
import android.util.JsonReader;
import android.view.View;

import org.junit.Assert;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.RenderCoordinatesImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.WebContents;

import java.io.IOException;
import java.io.StringReader;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Collection of DOM-based utilities.
 */
@JNINamespace("content")
public class DOMUtils {
    private static final long MEDIA_TIMEOUT_SECONDS = 10L;
    private static final long MEDIA_TIMEOUT_MILLISECONDS = MEDIA_TIMEOUT_SECONDS * 1000;

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
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    // Playback can't be reliably detected until current time moves forward.
                    return !DOMUtils.isMediaPaused(webContents, id)
                            && DOMUtils.getCurrentTime(webContents, id) > 0;
                } catch (TimeoutException e) {
                    // Intentionally do nothing
                    return false;
                }
            }
        }, MEDIA_TIMEOUT_MILLISECONDS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Waits until the playback of the media with given {@code id} has paused before ended.
     * @param webContents The WebContents in which the media element lives.
     * @param id The element's id to check.
     */
    public static void waitForMediaPauseBeforeEnd(final WebContents webContents, final String id) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return DOMUtils.isMediaPaused(webContents, id)
                            && !DOMUtils.isMediaEnded(webContents, id);
                } catch (TimeoutException e) {
                    // Intentionally do nothing
                    return false;
                }
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
     * Focus a DOM node by its id.
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
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static boolean clickNode(final WebContents webContents, String nodeId)
            throws TimeoutException {
        return clickNode(webContents, nodeId, true /* goThroughRootAndroidView */);
    }

    /**
     * Click a DOM node by its id, scrolling it into view first.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param goThroughRootAndroidView Whether the input should be routed through the Root View for
     *        the CVC.
     */
    public static boolean clickNode(final WebContents webContents, String nodeId,
            boolean goThroughRootAndroidView) throws TimeoutException {
        return clickNode(
                webContents, nodeId, goThroughRootAndroidView, true /* shouldScrollIntoView */);
    }

    /**
     * Click a DOM node by its id.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param goThroughRootAndroidView Whether the input should be routed through the Root View for
     *        the CVC.
     * @param shouldScrollIntoView Whether to scroll the node into view first.
     */
    public static boolean clickNode(final WebContents webContents, String nodeId,
            boolean goThroughRootAndroidView, boolean shouldScrollIntoView)
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
    public static void dragNodeTo(final WebContents webContents, String fromNodeId, String toNodeId,
            int stepCount, long downTime) throws TimeoutException {
        int[] fromTarget = getClickTargetForNodeByJs(
                webContents, "document.getElementById('" + fromNodeId + "')");
        int[] toTarget = getClickTargetForNodeByJs(
                webContents, "document.getElementById('" + toNodeId + "')");
        TouchCommon.dragTo(getActivity(webContents), fromTarget[0], fromTarget[1], toTarget[0],
                toTarget[1], stepCount, downTime);
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
    public static String getNodeField(String fieldName, final WebContents webContents,
            String nodeId) throws TimeoutException {
        return getNodeField(fieldName, webContents, nodeId, String.class);
    }

    /**
     * Wait until a given node has non-zero bounds.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     */
    public static void waitForNonZeroNodeBounds(
            final WebContents webContents, final String nodeId) {
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                try {
                    return !DOMUtils.getNodeBounds(webContents, nodeId).isEmpty();
                } catch (TimeoutException e) {
                    // Intentionally do nothing
                    return false;
                }
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
    public static <T> T getNodeField(String fieldName, final WebContents webContents, String nodeId,
            Class<T> valueType) throws TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  return [ node." + fieldName + " ];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
        Assert.assertFalse("Failed to retrieve contents for " + nodeId,
                jsonText.trim().equalsIgnoreCase("null"));
        return readValue(jsonText, valueType);
    }

    /**
     * Returns the value of a given attribute of type {@code valueType} as a {@code T}.
     * @param attributeName The attribute to return the value from.
     * @param webContents The WebContents in which the node lives.
     * @param nodeId The id of the node.
     * @param valueType The type of the value to read.
     * @return the attributes' value.
     */
    public static <T> T getNodeAttribute(String attributeName, final WebContents webContents,
            String nodeId, Class<T> valueType) throws InterruptedException, TimeoutException {
        StringBuilder sb = new StringBuilder();
        sb.append("(function() {");
        sb.append("  var node = document.getElementById('" + nodeId + "');");
        sb.append("  if (!node) return null;");
        sb.append("  return [ node.getAttribute('" + attributeName + "') ];");
        sb.append("})();");

        String jsonText =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, sb.toString());
        Assert.assertFalse("Failed to retrieve contents for " + nodeId,
                jsonText.trim().equalsIgnoreCase("null"));
        return readValue(jsonText, valueType);
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
        int clickY = (int) coord.fromLocalCssToPix(bounds.exactCenterY())
                + getMaybeTopControlsHeight(webContents);

        // This scale will almost always be 1. See the comments on
        // DisplayAndroid#getAndroidUIScaling().
        float scale = webContents.getTopLevelNativeWindow().getDisplay().getAndroidUIScaling();

        return new int[] {(int) (clickX * scale), (int) (clickY * scale)};
    }

    private static int getMaybeTopControlsHeight(final WebContents webContents) {
        try {
            return TestThreadUtils.runOnUiThreadBlocking(
                    () -> nativeGetTopControlsShrinkBlinkHeight(webContents));
        } catch (ExecutionException e) {
            return 0;
        }
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

        Assert.assertFalse("Failed to retrieve bounds for element: " + jsCode,
                jsonText.trim().equalsIgnoreCase("null"));

        JsonReader jsonReader = new JsonReader(new StringReader(jsonText));
        int[] bounds = new int[4];
        try {
            jsonReader.beginArray();
            int i = 0;
            while (jsonReader.hasNext()) {
                bounds[i++] = jsonReader.nextInt();
            }
            jsonReader.endArray();
            Assert.assertEquals("Invalid bounds returned.", 4, i);

            jsonReader.close();
        } catch (IOException exception) {
            Assert.fail("Failed to evaluate JavaScript: " + jsonText + "\n" + exception);
        }

        return new Rect(bounds[0], bounds[1], bounds[0] + bounds[2], bounds[1] + bounds[3]);
    }

    private static native int nativeGetTopControlsShrinkBlinkHeight(WebContents webContents);
}
