// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.UserData;
import org.chromium.content.browser.selection.SelectionPopupControllerImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsImpl.UserDataFactory;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/** Controls all the popup views on content view. */
public class PopupController implements UserData {
    /** Interface for popup views that expose a method for hiding itself. */
    public interface HideablePopup {
        /** Called when the popup needs to be hidden. */
        void hide();
    }

    private static final class UserDataFactoryLazyHolder {
        private static final UserDataFactory<PopupController> INSTANCE = PopupController::new;
    }

    private final List<HideablePopup> mHideablePopups = new ArrayList<>();

    public static PopupController fromWebContents(WebContents webContents) {
        return ((WebContentsImpl) webContents)
                .getOrSetUserData(PopupController.class, UserDataFactoryLazyHolder.INSTANCE);
    }

    private PopupController(WebContents webContents) {}

    /**
     * Hide all popup views.
     * @param webContents {@link WebContents} for current content.
     */
    public static void hideAll(WebContents webContents) {
        if (webContents == null) return;
        PopupController controller = PopupController.fromWebContents(webContents);
        if (controller != null) controller.hideAllPopups();
    }

    /**
     * Hide all popup views and clear text selection UI.
     * @param webContents {@link WebContents} for current content.
     */
    public static void hidePopupsAndClearSelection(WebContents webContents) {
        if (webContents == null) return;

        SelectionPopupControllerImpl controller =
                SelectionPopupControllerImpl.fromWebContentsNoCreate(webContents);
        if (controller != null) controller.destroyActionModeAndUnselect();
        PopupController.hideAll(webContents);
    }

    /**
     * Register a hideable popup.
     * @param webContents {@link WebContents} for current content.
     * @param popup {@link Hideable} popup view object.
     */
    public static void register(WebContents webContents, HideablePopup popup) {
        if (webContents == null) return;
        PopupController.fromWebContents(webContents).registerPopup(popup);
    }

    public void hideAllPopups() {
        for (HideablePopup popup : mHideablePopups) popup.hide();
    }

    public void registerPopup(HideablePopup popup) {
        mHideablePopups.add(popup);
    }
}
