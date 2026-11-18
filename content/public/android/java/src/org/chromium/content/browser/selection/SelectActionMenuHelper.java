// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.app.RemoteAction;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.textclassifier.TextClassification;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.base.StrictModeContext;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.R;
import org.chromium.content_public.browser.PendingSelectionMenu;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionClient.Result;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionMenuItem.ItemGroupOffset;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * Utility class around menu items for the text selection action menu. This was created (as opposed
 * to using a menu.xml) because we have multiple ways of rendering the menu that cannot necessarily
 * leverage the {@link android.view.Menu} & {@link MenuItem} APIs.
 */
@NullMarked
public class SelectActionMenuHelper {
    private static final String TAG = "SelectActionMenu"; // 20 char limit.

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ItemKeyShortcuts.CUT,
        ItemKeyShortcuts.COPY,
        ItemKeyShortcuts.PASTE,
        ItemKeyShortcuts.SELECT_ALL
    })
    public @interface ItemKeyShortcuts {
        char CUT = 'x';
        char COPY = 'c';
        char PASTE = 'v';
        char SELECT_ALL = 'a';
    }

    /** Delegate for determining which default actions can be taken. */
    public interface TextSelectionCapabilitiesDelegate {
        boolean canCut();

        boolean canCopy();

        boolean canPaste();

        boolean canShare(@MenuType int menuType);

        boolean canSelectAll(@MenuType int menuType);

        boolean canWebSearch(@MenuType int menuType);

        boolean canPasteAsPlainText();
    }

    /** Ephemeral class used to customize menu items as they're built and added to the menu. */
    @VisibleForTesting
    /* package */ static class ItemBuilder {
        private final @Nullable SelectionActionMenuDelegate mDelegate;
        private final @MenuType int mMenuType;
        private final boolean mIsSelectionReadOnly;
        private final String mSelectedText;

        /* package */ ItemBuilder(
                @Nullable SelectionActionMenuDelegate delegate,
                @MenuType int menuType,
                boolean isSelectionReadOnly,
                String selectedText) {
            mDelegate = delegate;
            mMenuType = menuType;
            mIsSelectionReadOnly = isSelectionReadOnly;
            mSelectedText = selectedText;
        }

        private @Nullable SelectionMenuItem buildItem(SelectionMenuItem.Builder pendingItem) {
            if (mDelegate != null) {
                return mDelegate.buildItem(
                        mMenuType, mIsSelectionReadOnly, mSelectedText, pendingItem);
            }
            return pendingItem.build();
        }
    }

    // Do not instantiate.
    private SelectActionMenuHelper() {}

    /** Removes all the menu item groups potentially added using {@link #getMenuItems}. */
    public static void removeAllAddedGroupsFromMenu(Menu menu) {
        // Only remove action mode items we added. See more http://crbug.com/709878.
        menu.removeGroup(R.id.select_action_menu_default_items);
        menu.removeGroup(R.id.select_action_menu_assist_items);
        menu.removeGroup(R.id.select_action_menu_text_processing_items);
        menu.removeGroup(android.R.id.textAssist);
    }

    /**
     * Returns all items for the text selection menu when there is text selected.
     *
     * @param delegate a delegate used to determine which default actions can be taken.
     * @param menu the pending menu used to build the context menu. Items should be added via add().
     * @param context the context used by the menu.
     * @param classificationResult the text classification result.
     * @param menuType whether the menu is a floating action mode menu or a dropdown menu.
     * @param isSelectionPassword true if the selection is a password.
     * @param isSelectionReadOnly true if the selection is non-editable.
     * @param selectedText the text that is currently selected for this menu.
     * @param isTextProcessingAllowed whether to show text processing items in this menu.
     * @param selectionActionMenuDelegate a delegate which can edit or add additional menu items.
     */
    public static void populateMenuItems(
            TextSelectionCapabilitiesDelegate delegate,
            PendingSelectionMenu menu,
            Context context,
            SelectionClient.@Nullable Result classificationResult,
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText,
            boolean isTextProcessingAllowed,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {

        // Add the groups in order. We sort the items on order when we create the menu and the sort
        // is stable so this ensures that the primary assist item will always appear first even if
        // another item has order 0.
        SelectionMenuItem primaryAssistItem =
                getPrimaryAssistItem(context, selectedText, classificationResult);
        if (primaryAssistItem != null) menu.addMenuItem(primaryAssistItem);
        ItemBuilder builder =
                new ItemBuilder(
                        selectionActionMenuDelegate, menuType, isSelectionReadOnly, selectedText);
        addDefaultItems(menu, context, delegate, menuType, builder, selectionActionMenuDelegate);
        addSecondaryAssistItems(menu, classificationResult);
        addTextProcessingItems(
                menu,
                context,
                menuType,
                isSelectionPassword,
                isSelectionReadOnly,
                selectedText,
                isTextProcessingAllowed,
                builder,
                selectionActionMenuDelegate);
        if (selectionActionMenuDelegate != null) {
            menu.addAll(
                    selectionActionMenuDelegate.getAdditionalMenuItems(
                            menuType, isSelectionPassword, isSelectionReadOnly, selectedText));
        }
    }

    private static @Nullable SelectionMenuItem getPrimaryAssistItem(
            Context context,
            String selectedText,
            SelectionClient.@Nullable Result classificationResult) {
        if (selectedText.isEmpty()) {
            return null;
        }
        if (classificationResult == null
                || classificationResult.textClassification == null
                || classificationResult.textClassification.getActions().isEmpty()) {
            return null;
        }
        RemoteAction primaryAction = classificationResult.textClassification.getActions().get(0);
        return new SelectionMenuItem.Builder(primaryAction.getTitle())
                .setId(android.R.id.textAssist)
                .setGroupId(R.id.select_action_menu_assist_items)
                .setOrderAndCategory(0, ItemGroupOffset.ASSIST_ITEMS)
                .setIcon(getPrimaryActionIconForClassificationResult(classificationResult, context))
                .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                .build();
    }

    @VisibleForTesting
    static void addDefaultItems(
            PendingSelectionMenu menu,
            @Nullable Context context,
            TextSelectionCapabilitiesDelegate delegate,
            @MenuType int menuType,
            ItemBuilder builder,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        // If the delegate is null, use the static default implementation. Otherwise call the method
        // on the delegate.
        @DefaultItem
        int[] itemOrder =
                selectionActionMenuDelegate == null
                        ? SelectionActionMenuDelegate.getDefaultMenuItemOrder()
                        : selectionActionMenuDelegate.getDefaultMenuItemOrder(menuType);
        for (int pos = 0; pos < itemOrder.length; pos++) {
            @DefaultItem int item = itemOrder[pos];
            switch (item) {
                case DefaultItem.CUT ->
                        menu.maybeAddMenuItem(builder.buildItem(cut(pos, delegate.canCut())));
                case DefaultItem.COPY ->
                        menu.maybeAddMenuItem(builder.buildItem(copy(pos, delegate.canCopy())));
                case DefaultItem.PASTE ->
                        menu.maybeAddMenuItem(builder.buildItem(paste(pos, delegate.canPaste())));
                case DefaultItem.PASTE_AS_PLAIN_TEXT ->
                        menu.maybeAddMenuItem(
                                builder.buildItem(
                                        pasteAsPlainText(
                                                context, pos, delegate.canPasteAsPlainText())));
                case DefaultItem.SHARE ->
                        menu.maybeAddMenuItem(
                                builder.buildItem(
                                        share(context, pos, delegate.canShare(menuType))));
                case DefaultItem.SELECT_ALL ->
                        menu.maybeAddMenuItem(
                                builder.buildItem(selectAll(pos, delegate.canSelectAll(menuType))));
                case DefaultItem.WEB_SEARCH ->
                        menu.maybeAddMenuItem(
                                builder.buildItem(webSearch(pos, delegate.canWebSearch(menuType))));
                default -> {}
            }
        }
    }

    private static void addSecondaryAssistItems(
            PendingSelectionMenu menu, @Nullable Result classificationResult) {
        // We have to use android.R.id.textAssist as group id to make framework show icons for
        // menu items if there is selected text.
        @IdRes int groupId = android.R.id.textAssist;

        if (classificationResult == null || classificationResult.textClassification == null) {
            return;
        }
        TextClassification classification = classificationResult.textClassification;
        List<RemoteAction> actions = classification.getActions();
        final int count = actions.size();
        if (count < 2) {
            // More than one item is needed as the first item is reserved for the
            // primary assist item.
            return;
        }
        List<Drawable> icons = classificationResult.additionalIcons;
        assert icons == null || icons.size() == count
                : "icons list should be either null or have the same length with actions.";

        // First action is reserved for primary action so start at index 1.
        final int startIndex = 1;
        for (int i = startIndex; i < count; i++) {
            RemoteAction action = actions.get(i);
            if (TextUtils.isEmpty(action.getTitle())) {
                Log.w(TAG, "Dropping selection menu item due to empty title.");
                continue;
            }

            menu.addMenuItem(
                    new SelectionMenuItem.Builder(action.getTitle())
                            .setId(Menu.NONE)
                            .setGroupId(groupId)
                            .setIcon(icons == null ? null : icons.get(i))
                            .setOrderAndCategory(
                                    i - startIndex, ItemGroupOffset.SECONDARY_ASSIST_ITEMS)
                            .setContentDescription(action.getContentDescription())
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .build());
        }
    }

    @VisibleForTesting
    /* package */ static void addTextProcessingItems(
            PendingSelectionMenu menu,
            Context context,
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText,
            boolean isTextProcessingAllowed,
            ItemBuilder builder,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        if (selectedText.isEmpty()) return;
        if (isSelectionPassword || !isTextProcessingAllowed) {
            return;
        }
        List<ResolveInfo> supportedActivities =
                PackageManagerUtils.queryIntentActivities(createProcessTextIntent(), 0);
        if (selectionActionMenuDelegate != null) {
            supportedActivities =
                    selectionActionMenuDelegate.filterTextProcessingActivities(
                            menuType, supportedActivities);
        }
        if (supportedActivities.isEmpty()) {
            return;
        }
        final PackageManager packageManager = context.getPackageManager();
        for (int i = 0; i < supportedActivities.size(); i++) {
            int category = ItemGroupOffset.TEXT_PROCESSING_ITEMS;
            int order = i;
            ResolveInfo resolveInfo = supportedActivities.get(i);
            if (resolveInfo.activityInfo == null || !resolveInfo.activityInfo.exported) continue;
            CharSequence title = resolveInfo.loadLabel(packageManager);
            Drawable icon;
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites();
                    StrictModeContext ignored2 = StrictModeContext.allowUnbufferedIo()) {
                icon = resolveInfo.loadIcon(packageManager);
            }
            Intent intent = createProcessTextIntentForResolveInfo(resolveInfo, isSelectionReadOnly);
            menu.maybeAddMenuItem(
                    builder.buildItem(
                            new SelectionMenuItem.Builder(title)
                                    .setId(Menu.NONE)
                                    .setGroupId(R.id.select_action_menu_text_processing_items)
                                    .setIcon(icon)
                                    .setOrderAndCategory(order, category)
                                    .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                                    .setIntent(intent)));
        }
    }

    private static Intent createProcessTextIntentForResolveInfo(
            ResolveInfo info, boolean isSelectionReadOnly) {
        return createProcessTextIntent()
                .putExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, isSelectionReadOnly)
                .setClassName(info.activityInfo.packageName, info.activityInfo.name);
    }

    private static Intent createProcessTextIntent() {
        return new Intent().setAction(Intent.ACTION_PROCESS_TEXT).setType("text/plain");
    }

    private static @Nullable Drawable getPrimaryActionIconForClassificationResult(
            SelectionClient.Result classificationResult, Context context) {
        final List<Drawable> additionalIcons = classificationResult.additionalIcons;
        Drawable icon;
        if (additionalIcons != null && !additionalIcons.isEmpty()) {
            // The primary action is always first so check index 0.
            icon = additionalIcons.get(0);
        } else {
            if (classificationResult.textClassification == null) return null;
            icon =
                    classificationResult
                            .textClassification
                            .getActions()
                            .get(0)
                            .getIcon()
                            .loadDrawable(context);
        }
        return icon;
    }

    private static SelectionMenuItem.Builder cut(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.cut)
                .setId(R.id.select_action_menu_cut)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeCutDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.CUT)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder copy(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.copy)
                .setId(R.id.select_action_menu_copy)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeCopyDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.COPY)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder paste(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.paste)
                .setId(android.R.id.paste)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModePasteDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.PASTE)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder share(
            @Nullable Context context, int order, boolean isEnabled) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        return new SelectionMenuItem.Builder(context.getString(R.string.actionbar_share))
                .setId(R.id.select_action_menu_share)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeShareDrawable)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder selectAll(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.selectAll)
                .setId(R.id.select_action_menu_select_all)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeSelectAllDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.SELECT_ALL)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder pasteAsPlainText(
            @Nullable Context context, int order, boolean isEnabled) {
        SelectionMenuItem.Builder builder =
                new SelectionMenuItem.Builder(android.R.string.paste_as_plain_text)
                        .setId(android.R.id.pasteAsPlainText)
                        .setGroupId(R.id.select_action_menu_default_items)
                        .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                        .setShowAsActionFlags(
                                MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                        .setIsEnabled(isEnabled);

        if (context != null) {
            builder.setIcon(ContextCompat.getDrawable(context, R.drawable.ic_paste_as_plain_text))
                    .setIsIconTintable(true);
        }
        return builder;
    }

    private static SelectionMenuItem.Builder webSearch(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(R.string.actionbar_web_search)
                .setId(R.id.select_action_menu_web_search)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeWebSearchDrawable)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }
}
