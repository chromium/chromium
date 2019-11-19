// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * Reference to the backend providing all the data.
 * @type {mojom.ProcessInternalsHandlerRemote}
 */
let pageHandler = null;

/**
 * @param {string} id Tab id.
 * @return {boolean} True if successful.
 */
function selectTab(id) {
  const tabContents = document.querySelectorAll('#content > div');
  const tabHeaders = $('navigation').querySelectorAll('.tab-header');
  let found = false;
  for (let i = 0; i < tabContents.length; i++) {
    const tabContent = tabContents[i];
    const tabHeader = tabHeaders[i];
    const isTargetTab = tabContent.id == id;

    found = found || isTargetTab;
    tabContent.classList.toggle('selected', isTargetTab);
    tabHeader.classList.toggle('selected', isTargetTab);
  }
  if (!found) {
    return false;
  }
  window.location.hash = id;
  return true;
}

function onHashChange() {
  const hash = window.location.hash.slice(1).toLowerCase();
  if (!selectTab(hash)) {
    selectTab('general');
  }
}

function setupTabs() {
  const tabContents = document.querySelectorAll('#content > div');
  for (let i = 0; i < tabContents.length; i++) {
    const tabContent = tabContents[i];
    const tabName = tabContent.querySelector('.content-header').textContent;

    const tabHeader = document.createElement('div');
    tabHeader.className = 'tab-header';
    const button = document.createElement('button');
    button.textContent = tabName;
    tabHeader.appendChild(button);
    tabHeader.addEventListener('click', selectTab.bind(null, tabContent.id));
    $('navigation').appendChild(tabHeader);
  }
  onHashChange();
}

/**
 * Root of the WebContents tree.
 * @type {cr.ui.Tree|null}
 */
let treeViewRoot = null;

/**
 * Initialize and return |treeViewRoot|.
 * @return {cr.ui.Tree} Initialized |treeViewRoot|.
 */
function getTreeViewRoot() {
  if (!treeViewRoot) {
    cr.ui.decorate('#tree-view', cr.ui.Tree);

    treeViewRoot = /** @type {cr.ui.Tree} */ ($('tree-view'));
    treeViewRoot.detail = {payload: {}, children: {}};
  }
  return treeViewRoot;
}

/**
 * Initialize and return a tree item representing a FrameInfo object and
 * recursively creates its subframe objects.
 * @param {mojom.FrameInfo} frame
 * @return {Array}
 */
function frameToTreeItem(frame) {
  // Compose the string which will appear in the entry for this frame.
  let itemLabel = `Frame[${frame.processId}:${frame.routingId}]:`;
  itemLabel += ` SI:${frame.siteInstance.id}`;
  if (frame.siteInstance.locked) {
    itemLabel += ', locked';
  }
  if (frame.siteInstance.siteUrl) {
    itemLabel += `, site:${frame.siteInstance.siteUrl.url}`;
  }
  if (frame.lastCommittedUrl) {
    itemLabel += ` | url: ${frame.lastCommittedUrl.url}`;
  }

  const item = new cr.ui.TreeItem(
      {label: itemLabel, detail: {payload: {}, children: {}}});
  item.mayHaveChildren_ = true;
  item.expanded = true;
  item.icon = '';

  let frameCount = 1;
  for (const subframe of frame.subframes) {
    const result = frameToTreeItem(subframe);
    const subItem = result[0];
    const count = result[1];

    frameCount += count;
    item.add(subItem);
  }

  return [item, frameCount];
}

/**
 * Initialize and return a tree item representing the WebContentsInfo object
 * and contains all frames in it as a subtree.
 * @param {mojom.WebContentsInfo} webContents
 * @return {!cr.ui.TreeItem}
 */
function webContentsToTreeItem(webContents) {
  let itemLabel = 'WebContents: ';
  if (webContents.title.length > 0) {
    itemLabel += webContents.title + ', ';
  }

  const item = new cr.ui.TreeItem(
      {label: itemLabel, detail: {payload: {}, children: {}}});
  item.mayHaveChildren_ = true;
  item.expanded = true;
  item.icon = '';

  const result = frameToTreeItem(webContents.rootFrame);
  const rootItem = result[0];
  const count = result[1];

  itemLabel += `${count} frame` + (count > 1 ? 's.' : '.');
  item.label = itemLabel;

  item.add(rootItem);
  return item;
}

/**
 * This is a callback which is invoked when the data for WebContents
 * associated with the browser profile is received from the browser process.
 * @param {mojom.ProcessInternalsHandler_GetAllWebContentsInfo_ResponseParams}
 *     input
 */
function populateWebContentsTab(input) {
  const tree = getTreeViewRoot();

  // Clear the tree first before populating it with the new content.
  tree.innerText = '';

  for (const webContents of input.infos) {
    const item = webContentsToTreeItem(webContents);
    tree.add(item);
  }
}

/**
 * Function which retrieves the data for all WebContents associated with the
 * current browser profile. The result is passed to populateWebContentsTab.
 */
function loadWebContentsInfo() {
  pageHandler.getAllWebContentsInfo().then(populateWebContentsTab);
}

/**
 * Function which retrieves the currently active isolated origins and inserts
 * them into the page.  It organizes these origins into two lists: persisted
 * isolated origins, which are triggered by password entry and apply only
 * within the current profile, and global isolated origins, which apply to all
 * profiles.
 */
function loadIsolatedOriginInfo() {
  // Retrieve any persistent isolated origins for the current profile. Insert
  // them into a list on the page if there is at least one such origin.
  pageHandler.getUserTriggeredIsolatedOrigins().then((response) => {
    const originCount = response.isolatedOrigins.length;
    if (!originCount) {
      return;
    }

    $('user-triggered-isolated-origins').textContent =
        'The following origins are isolated because you previously typed a ' +
        'password into these sites (' + originCount + ' total). ' +
        'Clear cookies or history to wipe this list; this takes effect ' +
        'after a restart.';

    const list = document.createElement('ul');
    for (const origin of response.isolatedOrigins) {
      const item = document.createElement('li');
      item.textContent = origin;
      list.appendChild(item);
    }

    $('user-triggered-isolated-origins').appendChild(list);
  });

  // Retrieve global isolated origins and insert them into a separate list if
  // there is at least one such origin.  Since these origins may come from
  // multiple sources, include the source info for each origin in parens.
  pageHandler.getGloballyIsolatedOrigins().then((response) => {
    const originCount = response.isolatedOrigins.length;
    if (!originCount) {
      return;
    }

    $('global-isolated-origins').textContent =
        'The following origins are isolated by default for all users (' +
        originCount + ' total).  A description of how each origin was ' +
        ' activated is provided in parentheses.';

    const list = document.createElement('ul');
    for (const originInfo of response.isolatedOrigins) {
      const item = document.createElement('li');
      item.textContent = `${originInfo.origin} (${originInfo.source})`;
      list.appendChild(item);
    }
    $('global-isolated-origins').appendChild(list);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup Mojo interface to the backend.
  pageHandler = mojom.ProcessInternalsHandler.getRemote(true);

  // Get the Site Isolation mode and populate it.
  pageHandler.getIsolationMode().then((response) => {
    $('isolation-mode').innerText = response.mode;
  });

  loadIsolatedOriginInfo();

  // Setup the tabbed UI
  setupTabs();

  // Start loading the information about WebContents.
  loadWebContentsInfo();

  $('refresh-button').addEventListener('click', loadWebContentsInfo);
});
})();
