// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {Tree, TreeItem} from 'chrome://resources/js/cr/ui/tree.js';
import {$} from 'chrome://resources/js/util.m.js';

import {FrameInfo, FrameInfo_Type, ProcessInternalsHandler, ProcessInternalsHandlerRemote, WebContentsInfo} from './process_internals.mojom-webui.js';

/**
 * Reference to the backend providing all the data.
 * @type {ProcessInternalsHandlerRemote}
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
    const isTargetTab = tabContent.id === id;

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
 * @type {Tree|null}
 */
let treeViewRoot = null;

/**
 * Initialize and return |treeViewRoot|.
 * @return {Tree} Initialized |treeViewRoot|.
 */
function getTreeViewRoot() {
  if (!treeViewRoot) {
    decorate('#tree-view', Tree);

    treeViewRoot = /** @type {Tree} */ ($('tree-view'));
    treeViewRoot.detail = {payload: {}, children: {}};
  }
  return treeViewRoot;
}

/**
 * Initialize and return a tree item representing a FrameInfo object and
 * recursively creates its subframe objects.
 * @param {FrameInfo} frame
 * @return {Array}
 */
function frameToTreeItem(frame) {
  // Compose the string which will appear in the entry for this frame.
  let itemLabel = `Frame[${frame.processId}:${frame.routingId}:${
    frame.agentSchedulingGroupId}]:`;
  if (frame.type === FrameInfo_Type.kBackForwardCache) {
    itemLabel += ` bfcached`;
  } else if (frame.type === FrameInfo_Type.kPrerender) {
    itemLabel += ` prerender`;
  }

  itemLabel += ` SI:${frame.siteInstance.id}`;
  if (frame.siteInstance.locked) {
    itemLabel += ', locked';
  }
  if (frame.siteInstance.siteUrl) {
    itemLabel += `, site:${frame.siteInstance.siteUrl.url}`;
  }
  if (frame.siteInstance.processLockUrl) {
    itemLabel += `, lock:${frame.siteInstance.processLockUrl.url}`;
  }
  if (frame.siteInstance.requiresOriginKeyedProcess) {
    itemLabel += ', origin-keyed';
  }
  if (frame.lastCommittedUrl) {
    itemLabel += ` | url: ${frame.lastCommittedUrl.url}`;
  }

  const item =
      new TreeItem({label: itemLabel, detail: {payload: {}, children: {}}});
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
 * @param {WebContentsInfo} webContents
 * @return {!TreeItem}
 */
function webContentsToTreeItem(webContents) {
  let itemLabel = 'WebContents: ';
  if (webContents.title.length > 0) {
    itemLabel += webContents.title + ', ';
  }

  const item =
      new TreeItem({label: itemLabel, detail: {payload: {}, children: {}}});
  item.mayHaveChildren_ = true;
  item.expanded = true;
  item.icon = '';

  const result = frameToTreeItem(webContents.rootFrame);
  const rootItem = result[0];
  const activeCount = result[1];
  item.add(rootItem);

  // Add data for all root nodes retrieved from back-forward cache.
  let cachedCount = 0;
  for (const cachedRoot of webContents.bfcachedRootFrames) {
    const cachedResult = frameToTreeItem(cachedRoot);
    item.add(cachedResult[0]);
    cachedCount++;
  }

  // Add data for all root nodes in prerendered pages.
  let prerenderCount = 0;
  for (const cachedRoot of webContents.prerenderRootFrames) {
    const cachedResult = frameToTreeItem(cachedRoot);
    item.add(cachedResult[0]);
    prerenderCount++;
  }

  // Builds a string according to English pluralization rules:
  // buildCountString(0, 'frame') => "0 frames"
  // buildCountString(1, 'frame') => "1 frame"
  // buildCountString(2, 'frame') => "2 frames"
  const buildCountString = ((count, name) => {
    return `${count} ${name}` + (count !== 1 ? 's' : '');
  });

  itemLabel += buildCountString(activeCount, 'active frame');
  if (cachedCount > 0) {
    itemLabel += ', ' + buildCountString(cachedCount, 'bfcached root');
  }
  if (prerenderCount > 0) {
    itemLabel += ', ' + buildCountString(prerenderCount, 'prerender root');
  }
  item.label = itemLabel;

  return item;
}

/**
 * This is a callback which is invoked when the data for WebContents
 * associated with the browser profile is received from the browser process.
 * @param {!Array<!WebContentsInfo>} infos
 */
function populateWebContentsTab(infos) {
  const tree = getTreeViewRoot();

  // Clear the tree first before populating it with the new content.
  tree.innerText = '';

  for (const webContents of infos) {
    const item = webContentsToTreeItem(webContents);
    tree.add(item);
  }
}

/**
 * Function which retrieves the data for all WebContents associated with the
 * current browser profile. The result is passed to populateWebContentsTab.
 */
async function loadWebContentsInfo() {
  const {infos} = await pageHandler.getAllWebContentsInfo();
  populateWebContentsTab(infos);
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
        'password or logged in on these sites (' + originCount + ' total). ' +
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

  pageHandler.getWebTriggeredIsolatedOrigins().then((response) => {
    const originCount = response.isolatedOrigins.length;
    if (!originCount) {
      return;
    }

    $('web-triggered-isolated-origins').textContent =
        'The following origins are isolated based on runtime heuristics ' +
        'triggered directly by web pages, such as Cross-Origin-Opener-Policy ' +
        'headers. Clear cookies or history to wipe this list; this takes ' +
        'effect after a restart.';

    const list = document.createElement('ul');
    for (const origin of response.isolatedOrigins) {
      const item = document.createElement('li');
      item.textContent = origin;
      list.appendChild(item);
    }

    $('web-triggered-isolated-origins').appendChild(list);
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
  pageHandler = ProcessInternalsHandler.getRemote();

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
