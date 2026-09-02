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
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Utility class around menu items for the text selection action menu. This was created (as opposed
 * to using a menu.xml) because we have multiple ways of rendering the menu that cannot necessarily
 * leverage the {@link android.view.Menu} & {@link MenuItem} APIs.
 */
@NullMarked
public class SelectActionMenuHelper {
    private static final String TAG = "SelectActionMenu"; // 20 char limit.

    /**
     * Spacing between consecutive default selection menu items within the {@link
     * SelectionMenuItem.ItemGroupOffset#DEFAULT_ITEMS} category. Default items are assigned orders
     * {@code 0, DEFAULT_ITEM_ORDER_SPACING, 2 * DEFAULT_ITEM_ORDER_SPACING, ...} based on their
     * position in the (possibly delegate-customized) order array, rather than consecutive integers.
     * This leaves {@code DEFAULT_ITEM_ORDER_SPACING - 1} free order slots in the gap between any
     * two consecutive default items so that embedders can interpose their own items at stable
     * positions without having to reorder the default items. See {@link
     * SelectionMenuItem.ItemOrder} for the interposition constants embedders use (e.g. {@link
     * SelectionMenuItem.ItemOrder#COPY_LINK_TO_HIGHLIGHT}, which lands in a gap between two default
     * items).
     */
    @VisibleForTesting static final int DEFAULT_ITEM_ORDER_SPACING = 10;

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

        menu.addAll(
                getDefaultItems(
                        context,
                        delegate,
                        menuType,
                        isSelectionReadOnly,
                        selectedText,
                        selectionActionMenuDelegate));

        // TODO(crbug.com/452918681): Instead of creating extra lists. We should pass the
        //  PendingSelectionMenu into these helper methods. This would require refactoring tests.
        List<SelectionMenuItem> secondaryAssistItems =
                getSecondaryAssistItems(classificationResult);
        if (secondaryAssistItems != null) menu.addAll(secondaryAssistItems);

        List<SelectionMenuItem> textProcessingAssistItems =
                getTextProcessingItems(
                        context,
                        menuType,
                        isSelectionPassword,
                        isSelectionReadOnly,
                        selectedText,
                        isTextProcessingAllowed,
                        selectionActionMenuDelegate);
        if (textProcessingAssistItems != null) {
            menu.addAll(textProcessingAssistItems);
        }
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
    static List<SelectionMenuItem> getDefaultItems(
            @Nullable Context context,
            TextSelectionCapabilitiesDelegate delegate,
            @MenuType int menuType,
            boolean isSelectionReadOnly,
            String selectedText,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        List<SelectionMenuItem> menuItems = new ArrayList<>();
        // If the delegate is null, use the static default implementation. Otherwise call the method
        // on the delegate.
        @DefaultItem
        int[] itemOrder =
                selectionActionMenuDelegate == null
                        ? SelectionActionMenuDelegate.getDefaultMenuItemOrder()
                        : selectionActionMenuDelegate.getDefaultMenuItemOrder(menuType);
        for (int pos = 0; pos < itemOrder.length; pos++) {
            @DefaultItem int item = itemOrder[pos];
            // Space default items out (see DEFAULT_ITEM_ORDER_SPACING) so embedders can interpose
            // their own items in the gaps between two default items at stable positions.
            int order = pos * DEFAULT_ITEM_ORDER_SPACING;
            if (item == DefaultItem.CUT) {
                if (menuType == MenuType.DROPDOWN ? !isSelectionReadOnly : delegate.canCut()) {
                    menuItems.add(cut(order, delegate.canCut()));
                }
            } else if (item == DefaultItem.COPY) {
                if (menuType == MenuType.DROPDOWN || delegate.canCopy()) {
                    menuItems.add(copy(order, delegate.canCopy()));
                }
            } else if (item == DefaultItem.PASTE) {
                if (menuType == MenuType.DROPDOWN ? !isSelectionReadOnly : delegate.canPaste()) {
                    menuItems.add(paste(order, delegate.canPaste()));
                }
            } else if (item == DefaultItem.PASTE_AS_PLAIN_TEXT) {
                if (menuType == MenuType.DROPDOWN
                        ? !isSelectionReadOnly
                        : delegate.canPasteAsPlainText()) {
                    menuItems.add(pasteAsPlainText(context, order, delegate.canPasteAsPlainText()));
                }
            } else if (item == DefaultItem.SHARE) {
                if (menuType == MenuType.DROPDOWN || delegate.canShare(menuType)) {
                    menuItems.add(
                            share(
                                    context,
                                    order,
                                    menuType,
                                    isSelectionReadOnly,
                                    delegate.canShare(menuType)));
                }
            } else if (item == DefaultItem.SELECT_ALL) {
                if (menuType == MenuType.DROPDOWN
                        ? !isSelectionReadOnly
                        : delegate.canSelectAll(menuType)) {
                    menuItems.add(selectAll(order, delegate.canSelectAll(menuType)));
                }
            } else if (item == DefaultItem.WEB_SEARCH) {
                if (menuType == MenuType.DROPDOWN || delegate.canWebSearch(menuType)) {
                    menuItems.add(
                            webSearch(
                                    context,
                                    order,
                                    selectedText,
                                    menuType,
                                    isSelectionReadOnly,
                                    selectionActionMenuDelegate,
                                    delegate.canWebSearch(menuType)));
                }
            }
        }
        return menuItems;
    }

    private static @Nullable List<SelectionMenuItem> getSecondaryAssistItems(
            @Nullable Result classificationResult) {
        // We have to use android.R.id.textAssist as group id to make framework show icons for
        // menu items if there is selected text.
        @IdRes int groupId = android.R.id.textAssist;

        if (classificationResult == null || classificationResult.textClassification == null) {
            return null;
        }
        TextClassification classification = classificationResult.textClassification;
        List<RemoteAction> actions = classification.getActions();
        final int count = actions.size();
        if (count < 2) {
            // More than one item is needed as the first item is reserved for the
            // primary assist item.
            return null;
        }
        List<Drawable> icons = classificationResult.additionalIcons;
        assert icons == null || icons.size() == count
                : "icons list should be either null or have the same length with actions.";

        List<SelectionMenuItem> secondaryAssistItems = new ArrayList<>();
        // First action is reserved for primary action so start at index 1.
        final int startIndex = 1;
        for (int i = startIndex; i < count; i++) {
            RemoteAction action = actions.get(i);
            if (TextUtils.isEmpty(action.getTitle())) {
                Log.w(TAG, "Dropping selection menu item due to empty title.");
                continue;
            }

            SelectionMenuItem item =
                    new SelectionMenuItem.Builder(action.getTitle())
                            .setId(Menu.NONE)
                            .setGroupId(groupId)
                            .setIcon(icons == null ? null : icons.get(i))
                            .setOrderAndCategory(
                                    i - startIndex, ItemGroupOffset.SECONDARY_ASSIST_ITEMS)
                            .setContentDescription(action.getContentDescription())
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .build();
            secondaryAssistItems.add(item);
        }
        return secondaryAssistItems;
    }

    @VisibleForTesting
    /* package */ static @Nullable List<SelectionMenuItem> getTextProcessingItems(
            Context context,
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText,
            boolean isTextProcessingAllowed,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        if (selectedText.isEmpty()) return null;
        List<SelectionMenuItem> textProcessingItems = new ArrayList<>();
        if (isSelectionPassword || !isTextProcessingAllowed) {
            return textProcessingItems;
        }
        List<ResolveInfo> supportedActivities =
                PackageManagerUtils.queryIntentActivities(createProcessTextIntent(), 0);
        if (selectionActionMenuDelegate != null) {
            supportedActivities =
                    selectionActionMenuDelegate.filterTextProcessingActivities(
                            menuType, supportedActivities);
        }
        if (supportedActivities.isEmpty()) {
            return textProcessingItems;
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
            if (isReadAloud(title)) {
                if (isSelectionReadOnly || menuType != MenuType.DROPDOWN) {
                    category = ItemGroupOffset.DEFAULT_ITEMS;
                    order = SelectionMenuItem.ItemOrder.READ_ALOUD_READ_ONLY;
                } else {
                    category = ItemGroupOffset.SECONDARY_ASSIST_ITEMS;
                    order = SelectionMenuItem.ItemOrder.READ_ALOUD_EDITABLE;
                }
            }
            textProcessingItems.add(
                    new SelectionMenuItem.Builder(title)
                            .setId(Menu.NONE)
                            .setGroupId(R.id.select_action_menu_text_processing_items)
                            .setIcon(icon)
                            .setOrderAndCategory(order, category)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .setIntent(intent)
                            .build());
        }
        return textProcessingItems;
    }

    private static boolean isReadAloud(@Nullable CharSequence title) {
        if (title != null) {
            String lowerTitle = title.toString().toLowerCase(Locale.US);
            if (lowerTitle.contains("read aloud")
                    || lowerTitle.contains("listen")
                    || lowerTitle.contains("reading mode")) {
                return true;
            }
        }
        return false;
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

    private static SelectionMenuItem cut(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.cut)
                .setId(R.id.select_action_menu_cut)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeCutDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.CUT)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true)
                .build();
    }

    private static SelectionMenuItem copy(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.copy)
                .setId(R.id.select_action_menu_copy)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeCopyDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.COPY)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true)
                .build();
    }

    private static SelectionMenuItem paste(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.paste)
                .setId(android.R.id.paste)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModePasteDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.PASTE)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true)
                .build();
    }

    private static SelectionMenuItem share(
            @Nullable Context context,
            int order,
            @MenuType int menuType,
            boolean isSelectionReadOnly,
            boolean isEnabled) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        @ItemGroupOffset int category = ItemGroupOffset.DEFAULT_ITEMS;
        if (!isSelectionReadOnly && menuType == MenuType.DROPDOWN) {
            category = ItemGroupOffset.SECONDARY_ASSIST_ITEMS;
            order = SelectionMenuItem.ItemOrder.SHARE_EDITABLE;
        }
        return new SelectionMenuItem.Builder(context.getString(R.string.actionbar_share))
                .setId(R.id.select_action_menu_share)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeShareDrawable)
                .setOrderAndCategory(order, category)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true)
                .build();
    }

    private static SelectionMenuItem selectAll(int order, boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.selectAll)
                .setId(R.id.select_action_menu_select_all)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeSelectAllDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.SELECT_ALL)
                .setOrderAndCategory(order, ItemGroupOffset.DEFAULT_ITEMS)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true)
                .build();
    }

    private static SelectionMenuItem pasteAsPlainText(
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
        return builder.build();
    }

    private static SelectionMenuItem webSearch(
            @Nullable Context context,
            int order,
            String selectedText,
            @MenuType int menuType,
            boolean isSelectionReadOnly,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate,
            boolean isEnabled) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        // Limited the title, "Search <default search engine> for <selected text>", to context menu
        // triggered by cursor right click (or touchpad double tap) only. Otherwise using default
        // title "Web Search".
        String title = context.getString(R.string.actionbar_web_search);
        if (menuType == MenuType.DROPDOWN && selectionActionMenuDelegate != null) {
            String customTitle =
                    selectionActionMenuDelegate.getWebSearchMenuItemTitle(context, selectedText);
            if (customTitle != null) {
                title = customTitle;
            }
        }
        @ItemGroupOffset int category = ItemGroupOffset.DEFAULT_ITEMS;
        if (!isSelectionReadOnly && menuType == MenuType.DROPDOWN) {
            category = ItemGroupOffset.SECONDARY_ASSIST_ITEMS;
            order = SelectionMenuItem.ItemOrder.WEB_SEARCH_EDITABLE;
        }
        return new SelectionMenuItem.Builder(title)
                .setId(R.id.select_action_menu_web_search)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeWebSearchDrawable)
                .setOrderAndCategory(order, category)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true)
                .build();
    }
}
