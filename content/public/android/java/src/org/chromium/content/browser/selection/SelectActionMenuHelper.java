// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.selection;

import android.app.ActivityOptions;
import android.app.PendingIntent;
import android.app.RemoteAction;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.textclassifier.TextClassification;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content.R;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionClient.Result;
import org.chromium.content_public.browser.SelectionMenuGroup;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.SortedSet;
import java.util.TreeSet;

/**
 * Utility class around menu items for the text selection action menu.
 * This was created (as opposed to using a menu.xml) because we have multiple ways of rendering the
 * menu that cannot necessarily leverage the {@link android.view.Menu} & {@link MenuItem} APIs.
 */
@NullMarked
public class SelectActionMenuHelper {
    private static final String TAG = "SelectActionMenu"; // 20 char limit.

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        GroupItemOrder.ASSIST_ITEMS,
        GroupItemOrder.DEFAULT_ITEMS,
        GroupItemOrder.SECONDARY_ASSIST_ITEMS,
        GroupItemOrder.TEXT_PROCESSING_ITEMS
    })
    public @interface GroupItemOrder {
        int ASSIST_ITEMS = 1;
        int DEFAULT_ITEMS = 2;
        int SECONDARY_ASSIST_ITEMS = 3;
        int TEXT_PROCESSING_ITEMS = 4;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        DefaultItemOrder.CUT,
        DefaultItemOrder.COPY,
        DefaultItemOrder.PASTE,
        DefaultItemOrder.PASTE_AS_PLAIN_TEXT,
        DefaultItemOrder.SHARE,
        DefaultItemOrder.SELECT_ALL,
        DefaultItemOrder.WEB_SEARCH
    })
    public @interface DefaultItemOrder {
        int CUT = 1;
        int COPY = 2;
        int PASTE = 3;
        int PASTE_AS_PLAIN_TEXT = 4;
        int SHARE = 5;
        int SELECT_ALL = 6;
        int WEB_SEARCH = 7;
    }

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

        boolean canShare();

        boolean canSelectAll();

        boolean canWebSearch();

        boolean canPasteAsPlainText();
    }

    /** For the text processing menu items. */
    public interface TextProcessingIntentHandler {
        void handleIntent(Intent textProcessingIntent);
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
     * @param context the context used by the menu.
     * @param classificationResult the text classification result.
     * @param isSelectionPassword true if the selection is a password.
     * @param isSelectionReadOnly true if the selection is non-editable.
     * @param selectedText the text that is currently selected for this menu.
     * @param textProcessingIntentHandler the intent handler for text processing actions.
     * @param selectionActionMenuDelegate a delegate which can edit or add additional menu items.
     */
    public static SortedSet<SelectionMenuGroup> getMenuItems(
            TextSelectionCapabilitiesDelegate delegate,
            Context context,
            SelectionClient.@Nullable Result classificationResult,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText,
            @Nullable TextProcessingIntentHandler textProcessingIntentHandler,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        SortedSet<SelectionMenuGroup> itemGroups = new TreeSet<>();

        itemGroups.add(
                getDefaultItems(
                        context,
                        delegate,
                        selectionActionMenuDelegate,
                        isSelectionPassword,
                        isSelectionReadOnly,
                        selectedText));

        SelectionMenuGroup primaryAssistItems =
                getPrimaryAssistItems(context, selectedText, classificationResult);
        if (primaryAssistItems != null) itemGroups.add(primaryAssistItems);

        SelectionMenuGroup secondaryAssistItems =
                getSecondaryAssistItems(
                        selectionActionMenuDelegate, classificationResult, selectedText);
        if (secondaryAssistItems != null) itemGroups.add(secondaryAssistItems);

        SelectionMenuGroup textProcessingAssistItems =
                getTextProcessingItems(
                        context,
                        isSelectionPassword,
                        isSelectionReadOnly,
                        selectedText,
                        textProcessingIntentHandler,
                        selectionActionMenuDelegate);
        if (textProcessingAssistItems != null) {
            itemGroups.add(textProcessingAssistItems);
        }

        return itemGroups;
    }

    private static @Nullable SelectionMenuGroup getPrimaryAssistItems(
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
        SelectionMenuGroup primaryAssistGroup =
                new SelectionMenuGroup(
                        R.id.select_action_menu_assist_items, GroupItemOrder.ASSIST_ITEMS);
        RemoteAction primaryAction = classificationResult.textClassification.getActions().get(0);
        primaryAssistGroup.addItem(
                new SelectionMenuItem.Builder(primaryAction.getTitle())
                        .setId(android.R.id.textAssist)
                        .setGroupId(R.id.select_action_menu_assist_items)
                        .setIcon(
                                getPrimaryActionIconForClassificationResult(
                                        classificationResult, context))
                        .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                        .setClickListener(getActionClickListener(primaryAction))
                        .build());
        return primaryAssistGroup;
    }

    @VisibleForTesting
    static SelectionMenuGroup getDefaultItems(
            @Nullable Context context,
            TextSelectionCapabilitiesDelegate delegate,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        SelectionMenuGroup defaultGroup =
                new SelectionMenuGroup(
                        R.id.select_action_menu_default_items, GroupItemOrder.DEFAULT_ITEMS);
        List<SelectionMenuItem.Builder> menuItemBuilders = new ArrayList<>();
        if (delegate.canCut()) menuItemBuilders.add(cut());
        if (delegate.canCopy()) menuItemBuilders.add(copy());
        if (delegate.canPaste()) menuItemBuilders.add(paste());
        if (delegate.canShare()) menuItemBuilders.add(share(context));
        if (delegate.canSelectAll()) menuItemBuilders.add(selectAll());
        if (delegate.canWebSearch()) menuItemBuilders.add(webSearch(context));
        if (delegate.canPasteAsPlainText()) menuItemBuilders.add(pasteAsPlainText(context));
        if (selectionActionMenuDelegate != null) {
            selectionActionMenuDelegate.modifyDefaultMenuItems(
                    menuItemBuilders, isSelectionPassword, isSelectionReadOnly, selectedText);
        }
        for (SelectionMenuItem.Builder builder : menuItemBuilders) {
            defaultGroup.addItem(builder.build());
        }
        return defaultGroup;
    }

    private static @Nullable SelectionMenuGroup getSecondaryAssistItems(
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate,
            @Nullable Result classificationResult,
            String selectedText) {
        // We have to use android.R.id.textAssist as group id to make framework show icons for
        // menu items if there is selected text.
        @IdRes int groupId = android.R.id.textAssist;
        SelectionMenuGroup secondaryAssistItems =
                new SelectionMenuGroup(groupId, GroupItemOrder.SECONDARY_ASSIST_ITEMS);

        if (selectedText.isEmpty() && selectionActionMenuDelegate != null) {
            List<SelectionMenuItem> additionalMenuItems =
                    selectionActionMenuDelegate.getAdditionalNonSelectionItems();
            if (!additionalMenuItems.isEmpty()) {
                secondaryAssistItems.addItems(additionalMenuItems);
                return secondaryAssistItems;
            }
        }
        if (classificationResult == null) {
            return null;
        }
        TextClassification classification = classificationResult.textClassification;
        if (classification == null) {
            return null;
        }
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

        // First action is reserved for primary action so start at index 1.
        final int startIndex = 1;
        for (int i = startIndex; i < count; i++) {
            RemoteAction action = actions.get(i);
            final View.OnClickListener listener = getActionClickListener(action);
            if (listener == null) continue;

            SelectionMenuItem item =
                    new SelectionMenuItem.Builder(action.getTitle())
                            .setId(Menu.NONE)
                            .setGroupId(groupId)
                            .setIcon(icons == null ? null : icons.get(i))
                            .setOrderInCategory(i - startIndex)
                            .setContentDescription(action.getContentDescription())
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .setClickListener(listener)
                            .build();
            secondaryAssistItems.addItem(item);
        }
        return secondaryAssistItems;
    }

    @VisibleForTesting
    /* package */ static @Nullable SelectionMenuGroup getTextProcessingItems(
            Context context,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText,
            @Nullable TextProcessingIntentHandler intentHandler,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        if (selectedText.isEmpty()) return null;
        SelectionMenuGroup textProcessingItems =
                new SelectionMenuGroup(
                        R.id.select_action_menu_text_processing_items,
                        GroupItemOrder.TEXT_PROCESSING_ITEMS);
        if (isSelectionPassword || intentHandler == null) {
            addAdditionalTextProcessingItems(textProcessingItems, selectionActionMenuDelegate);
            return textProcessingItems;
        }
        List<ResolveInfo> supportedActivities =
                PackageManagerUtils.queryIntentActivities(createProcessTextIntent(), 0);
        if (selectionActionMenuDelegate != null) {
            supportedActivities =
                    selectionActionMenuDelegate.filterTextProcessingActivities(supportedActivities);
        }
        if (supportedActivities.isEmpty()) {
            addAdditionalTextProcessingItems(textProcessingItems, selectionActionMenuDelegate);
            return textProcessingItems;
        }
        final PackageManager packageManager = context.getPackageManager();
        for (int i = 0; i < supportedActivities.size(); i++) {
            ResolveInfo resolveInfo = supportedActivities.get(i);
            if (resolveInfo.activityInfo == null || !resolveInfo.activityInfo.exported) continue;
            CharSequence title = resolveInfo.loadLabel(packageManager);
            Drawable icon;
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites();
                    StrictModeContext ignored2 = StrictModeContext.allowUnbufferedIo()) {
                icon = resolveInfo.loadIcon(packageManager);
            }
            Intent intent = createProcessTextIntentForResolveInfo(resolveInfo, isSelectionReadOnly);
            View.OnClickListener listener = v -> intentHandler.handleIntent(intent);
            textProcessingItems.addItem(
                    new SelectionMenuItem.Builder(title)
                            .setId(Menu.NONE)
                            .setGroupId(R.id.select_action_menu_text_processing_items)
                            .setIcon(icon)
                            .setOrderInCategory(i)
                            .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                            .setClickListener(listener)
                            .setIntent(intent)
                            .build());
        }
        addAdditionalTextProcessingItems(textProcessingItems, selectionActionMenuDelegate);
        return textProcessingItems;
    }

    private static void addAdditionalTextProcessingItems(
            SelectionMenuGroup textProcessingItems,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
        if (selectionActionMenuDelegate != null) {
            textProcessingItems.addItems(
                    selectionActionMenuDelegate.getAdditionalTextProcessingItems());
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

    private static View.@Nullable OnClickListener getActionClickListener(RemoteAction action) {
        if (TextUtils.isEmpty(action.getTitle())) {
            return null;
        }
        return v -> {
            try {
                ActivityOptions options = ActivityOptions.makeBasic();
                ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartAllowAlways(options);
                action.getActionIntent()
                        .send(
                                ContextUtils.getApplicationContext(),
                                0,
                                null,
                                null,
                                null,
                                null,
                                options.toBundle());
            } catch (PendingIntent.CanceledException e) {
                Log.e(TAG, "Error creating OnClickListener from PendingIntent", e);
            }
        };
    }

    private static SelectionMenuItem.Builder cut() {
        return new SelectionMenuItem.Builder(android.R.string.cut)
                .setId(R.id.select_action_menu_cut)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeCutDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.CUT)
                .setOrderInCategory(DefaultItemOrder.CUT)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(true)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder copy() {
        return new SelectionMenuItem.Builder(android.R.string.copy)
                .setId(R.id.select_action_menu_copy)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeCopyDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.COPY)
                .setOrderInCategory(DefaultItemOrder.COPY)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(true)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder paste() {
        return new SelectionMenuItem.Builder(android.R.string.paste)
                .setId(R.id.select_action_menu_paste)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModePasteDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.PASTE)
                .setOrderInCategory(DefaultItemOrder.PASTE)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(true)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder share(@Nullable Context context) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        return new SelectionMenuItem.Builder(context.getString(R.string.actionbar_share))
                .setId(R.id.select_action_menu_share)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeShareDrawable)
                .setOrderInCategory(DefaultItemOrder.SHARE)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(true)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder selectAll() {
        return new SelectionMenuItem.Builder(android.R.string.selectAll)
                .setId(R.id.select_action_menu_select_all)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeSelectAllDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.SELECT_ALL)
                .setOrderInCategory(DefaultItemOrder.SELECT_ALL)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(true)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder pasteAsPlainText(@Nullable Context context) {
        SelectionMenuItem.Builder builder =
                new SelectionMenuItem.Builder(android.R.string.paste_as_plain_text)
                        .setId(R.id.select_action_menu_paste_as_plain_text)
                        .setGroupId(R.id.select_action_menu_default_items)
                        .setOrderInCategory(DefaultItemOrder.PASTE_AS_PLAIN_TEXT)
                        .setShowAsActionFlags(
                                MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                        .setIsEnabled(true);

        if (context != null) {
            builder.setIcon(ContextCompat.getDrawable(context, R.drawable.ic_paste_as_plain_text))
                    .setIsIconTintable(true);
        }
        return builder;
    }

    private static SelectionMenuItem.Builder webSearch(@Nullable Context context) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        return new SelectionMenuItem.Builder(context.getString(R.string.actionbar_web_search))
                .setId(R.id.select_action_menu_web_search)
                .setGroupId(R.id.select_action_menu_default_items)
                .setIconAttr(android.R.attr.actionModeWebSearchDrawable)
                .setOrderInCategory(DefaultItemOrder.WEB_SEARCH)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(true)
                .setIsIconTintable(true);
    }
}
