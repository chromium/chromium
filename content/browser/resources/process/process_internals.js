// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

/**
 * Reference to the backend providing all the data.
 * @type {mojom.ProcessInternalsHandlerPtr}
 */
let uiHandler = null;

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
  if (!found)
    return false;
  window.location.hash = id;
  return true;
}

function onHashChange() {
  let hash = window.location.hash.slice(1).toLowerCase();
  if (!selectTab(hash))
    selectTab('general');
}

function setupTabs() {
  const tabContents = document.querySelectorAll('#content > div');
  for (let i = 0; i < tabContents.length; i++) {
    const tabContent = tabContents[i];
    const tabName = tabContent.querySelector('.content-header').textContent;

    let tabHeader = document.createElement('div');
    tabHeader.className = 'tab-header';
    let button = document.createElement('button');
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
  if (frame.siteInstance.locked)
    itemLabel += ', locked';
  if (frame.siteInstance.siteUrl)
    itemLabel += `, site:${frame.siteInstance.siteUrl.url}`;
  if (frame.lastCommittedUrl)
    itemLabel += ` | url: ${frame.lastCommittedUrl.url}`;

  let item = new cr.ui.TreeItem(
      {label: itemLabel, detail: {payload: {}, children: {}}});
  item.mayHaveChildren_ = true;
  item.expanded = true;
  item.icon = '';

  let frameCount = 1;
  for (const subframe of frame.subframes) {
    let result = frameToTreeItem(subframe);
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
  if (webContents.title.length > 0)
    itemLabel += webContents.title + ', ';

  let item = new cr.ui.TreeItem(
      {label: itemLabel, detail: {payload: {}, children: {}}});
  item.mayHaveChildren_ = true;
  item.expanded = true;
  item.icon = '';

  let result = frameToTreeItem(webContents.rootFrame);
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
  let tree = getTreeViewRoot();

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
  uiHandler.getAllWebContentsInfo().then(populateWebContentsTab);
}

document.addEventListener('DOMContentLoaded', function() {
  // Setup Mojo interface to the backend.
  uiHandler = new mojom.ProcessInternalsHandlerPtr;
  Mojo.bindInterface(
      mojom.ProcessInternalsHandler.name, mojo.makeRequest(uiHandler).handle);

  // Get the Site Isolation mode and populate it.
  uiHandler.getIsolationMode().then((response) => {
    $('isolation-mode').innerText = response.mode;
  });
  uiHandler.getIsolatedOriginsSize().then((response) => {
    $('isolated-origins').innerText = response.size;
  });

  // Setup the tabbed UI
  setupTabs();

  // Start loading the information about WebContents.
  loadWebContentsInfo();

  $('refresh-button').addEventListener('click', loadWebContentsInfo);
});
})();
