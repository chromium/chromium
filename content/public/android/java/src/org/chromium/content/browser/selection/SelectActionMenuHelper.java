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
import android.os.Build;
import android.text.TextUtils;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.textclassifier.TextClassification;

import androidx.annotation.IdRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.content.R;
import org.chromium.content_public.browser.ContentFeatureMap;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionClient.Result;
import org.chromium.content_public.browser.SelectionMenuGroup;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;
import org.chromium.content_public.common.ContentFeatures;

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

    /** Delegate for the select action menu. */
    public interface SelectActionMenuDelegate {
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
     * @param context the context used by the menu.
     * @param classificationResult the text classification result.
     * @param isSelectionReadOnly true if the selection is non-editable.
     * @param textProcessingIntentHandler the intent handler for text processing actions.
     */
    public static SortedSet<SelectionMenuGroup> getMenuItems(
            SelectActionMenuDelegate delegate,
            Context context,
            @Nullable SelectionClient.Result classificationResult,
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
                        selectedText));

        SelectionMenuGroup primaryAssistItems =
                getPrimaryAssistItems(context, selectedText, classificationResult);
        if (primaryAssistItems != null) itemGroups.add(primaryAssistItems);

        SelectionMenuGroup secondaryAssistItems =
                getSecondaryAssistItems(
                        selectionActionMenuDelegate, classificationResult, selectedText);
        if (secondaryAssistItems != null) itemGroups.add(secondaryAssistItems);

        itemGroups.add(
                getTextProcessingItems(
                        context,
                        isSelectionPassword,
                        isSelectionReadOnly,
                        textProcessingIntentHandler,
                        selectionActionMenuDelegate));

        return itemGroups;
    }

    @Nullable
    @RequiresApi(Build.VERSION_CODES.O)
    private static SelectionMenuGroup getPrimaryAssistItems(
            Context context,
            String selectedText,
            @Nullable SelectionClient.Result classificationResult) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O || selectedText.isEmpty()) {
            return null;
        }
        if (classificationResult == null || !classificationResult.hasNamedAction()) {
            return null;
        }
        SelectionMenuGroup primaryAssistGroup =
                new SelectionMenuGroup(
                        R.id.select_action_menu_assist_items, GroupItemOrder.ASSIST_ITEMS);
        View.OnClickListener clickListener = null;
        if (classificationResult.onClickListener != null) {
            clickListener = classificationResult.onClickListener;
        } else if (classificationResult.intent != null) {
            clickListener = v -> context.startActivity(classificationResult.intent);
        }
        primaryAssistGroup.addItem(
                new SelectionMenuItem.Builder(classificationResult.label)
                        .setId(android.R.id.textAssist)
                        .setIcon(getPrimaryActionIconForClassificationResult(classificationResult))
                        .setShowAsActionFlags(MenuItem.SHOW_AS_ACTION_IF_ROOM)
                        .setClickListener(clickListener)
                        .build());
        return primaryAssistGroup;
    }

    @VisibleForTesting
    static SelectionMenuGroup getDefaultItems(
            @Nullable Context context,
            SelectActionMenuDelegate delegate,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate,
            boolean isSelectionPassword,
            String selectedText) {
        SelectionMenuGroup defaultGroup =
                new SelectionMenuGroup(
                        R.id.select_action_menu_default_items, GroupItemOrder.DEFAULT_ITEMS);
        List<SelectionMenuItem.Builder> menuItemBuilders = new ArrayList<>();
        menuItemBuilders.add(cut(delegate.canCut()));
        menuItemBuilders.add(copy(delegate.canCopy()));
        menuItemBuilders.add(paste(delegate.canPaste()));
        menuItemBuilders.add(share(context, delegate.canShare()));
        menuItemBuilders.add(selectAll(delegate.canSelectAll()));
        menuItemBuilders.add(webSearch(context, delegate.canWebSearch()));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            menuItemBuilders.add(pasteAsPlainText(context, delegate.canPasteAsPlainText()));
        }
        if (ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION)
                && selectionActionMenuDelegate != null) {
            selectionActionMenuDelegate.modifyDefaultMenuItems(
                    menuItemBuilders, isSelectionPassword, selectedText);
        }
        for (SelectionMenuItem.Builder builder : menuItemBuilders) {
            defaultGroup.addItem(builder.build());
        }
        return defaultGroup;
    }

    @Nullable
    private static SelectionMenuGroup getSecondaryAssistItems(
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate,
            @Nullable Result classificationResult,
            String selectedText) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return null;
        // We have to use android.R.id.textAssist as group id to make framework show icons for
        // menu items if there is selected text.
        @IdRes int groupId = selectedText.isEmpty() ? Menu.NONE : android.R.id.textAssist;
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
        if (actions == null) {
            return null;
        }
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
    /* package */ static SelectionMenuGroup getTextProcessingItems(
            Context context,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            @Nullable TextProcessingIntentHandler intentHandler,
            @Nullable SelectionActionMenuDelegate selectionActionMenuDelegate) {
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
        if (ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION) &&
                selectionActionMenuDelegate != null) {
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
            SelectionActionMenuDelegate selectionActionMenuDelegate) {
        if (ContentFeatureMap.isEnabled(ContentFeatures.SELECTION_MENU_ITEM_MODIFICATION)
                && selectionActionMenuDelegate != null) {
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

    @Nullable
    private static Drawable getPrimaryActionIconForClassificationResult(
            SelectionClient.Result classificationResult) {
        final List<Drawable> additionalIcons = classificationResult.additionalIcons;
        Drawable icon;
        if (additionalIcons != null && !additionalIcons.isEmpty()) {
            // The primary action is always first so check index 0.
            icon = additionalIcons.get(0);
        } else {
            icon = classificationResult.icon;
        }
        return icon;
    }

    @Nullable
    @RequiresApi(Build.VERSION_CODES.O)
    private static View.OnClickListener getActionClickListener(RemoteAction action) {
        if (TextUtils.isEmpty(action.getTitle()) || action.getActionIntent() == null) {
            return null;
        }
        return v -> {
            try {
                ActivityOptions options = ActivityOptions.makeBasic();
                ApiCompatibilityUtils.setActivityOptionsBackgroundActivityStartMode(options);
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

    private static SelectionMenuItem.Builder cut(boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.cut)
                .setId(R.id.select_action_menu_cut)
                .setIconAttr(android.R.attr.actionModeCutDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.CUT)
                .setOrderInCategory(DefaultItemOrder.CUT)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder copy(boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.copy)
                .setId(R.id.select_action_menu_copy)
                .setIconAttr(android.R.attr.actionModeCopyDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.COPY)
                .setOrderInCategory(DefaultItemOrder.COPY)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder paste(boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.paste)
                .setId(R.id.select_action_menu_paste)
                .setIconAttr(android.R.attr.actionModePasteDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.PASTE)
                .setOrderInCategory(DefaultItemOrder.PASTE)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder share(@Nullable Context context, boolean isEnabled) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        return new SelectionMenuItem.Builder(context.getString(R.string.actionbar_share))
                .setId(R.id.select_action_menu_share)
                .setIconAttr(android.R.attr.actionModeShareDrawable)
                .setOrderInCategory(DefaultItemOrder.SHARE)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    private static SelectionMenuItem.Builder selectAll(boolean isEnabled) {
        return new SelectionMenuItem.Builder(android.R.string.selectAll)
                .setId(R.id.select_action_menu_select_all)
                .setIconAttr(android.R.attr.actionModeSelectAllDrawable)
                .setAlphabeticShortcut(ItemKeyShortcuts.SELECT_ALL)
                .setOrderInCategory(DefaultItemOrder.SELECT_ALL)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private static SelectionMenuItem.Builder pasteAsPlainText(
            @Nullable Context context, boolean isEnabled) {
        SelectionMenuItem.Builder builder =
                new SelectionMenuItem.Builder(android.R.string.paste_as_plain_text)
                        .setId(R.id.select_action_menu_paste_as_plain_text)
                        .setOrderInCategory(DefaultItemOrder.PASTE_AS_PLAIN_TEXT)
                        .setShowAsActionFlags(
                                MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                        .setIsEnabled(isEnabled);

        if (context != null) {
            builder.setIcon(ContextCompat.getDrawable(context, R.drawable.ic_paste_as_plain_text))
                    .setIsIconTintable(true);
        }
        return builder;
    }

    private static SelectionMenuItem.Builder webSearch(
            @Nullable Context context, boolean isEnabled) {
        if (context == null) {
            context = ContextUtils.getApplicationContext();
        }
        return new SelectionMenuItem.Builder(context.getString(R.string.actionbar_web_search))
                .setId(R.id.select_action_menu_web_search)
                .setIconAttr(android.R.attr.actionModeWebSearchDrawable)
                .setOrderInCategory(DefaultItemOrder.WEB_SEARCH)
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setIsEnabled(isEnabled)
                .setIsIconTintable(true);
    }
}
