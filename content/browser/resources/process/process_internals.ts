// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tree/cr_tree.js';

import type {CrTreeElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree.js';
import type {CrTreeItemElement} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import {MAY_HAVE_CHILDREN_ATTR} from 'chrome://resources/cr_elements/cr_tree/cr_tree_item.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {FrameInfo, ProcessInternalsHandlerRemote, WebContentsInfo} from './process_internals.mojom-webui.js';
import {FrameInfo_Type, ProcessInternalsHandler} from './process_internals.mojom-webui.js';

/**
 * Reference to the backend providing all the data.
 */
let pageHandler: ProcessInternalsHandlerRemote|null = null;

/**
 * @return True if successful.
 */
function selectTab(id: string): boolean {
  const tabContents = document.querySelectorAll<HTMLElement>('#content > div');
  const navigation = document.querySelector<HTMLElement>('#navigation');
  assert(navigation);
  const tabHeaders = navigation.querySelectorAll<HTMLElement>('.tab-header');
  let found = false;
  for (let i = 0; i < tabContents.length; i++) {
    const tabContent = tabContents[i]!;
    const tabHeader = tabHeaders[i]!;
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
  for (const tabContent of tabContents) {
    const contentHeader =
        tabContent.querySelector<HTMLElement>('.content-header');
    assert(contentHeader);
    const tabName = contentHeader.textContent;

    const tabHeader = document.createElement('div');
    tabHeader.className = 'tab-header';
    const button = document.createElement('button');
    button.textContent = tabName;
    tabHeader.appendChild(button);
    tabHeader.addEventListener('click', selectTab.bind(null, tabContent.id));
    const navigation = document.querySelector('#navigation');
    assert(navigation);
    navigation.appendChild(tabHeader);
  }
  onHashChange();
}

/**
 * Collects and displays info about the renderer process count and limit across
 * all profiles.
 */
async function loadProcessCountInfo() {
  assert(pageHandler);
  const {info} = await pageHandler.getProcessCountInfo();

  const processCountTotal =
      document.querySelector<HTMLElement>('#process-count-total');
  assert(processCountTotal);
  processCountTotal.innerText = String(info.rendererProcessCountTotal);

  const processCountForLimit =
      document.querySelector<HTMLElement>('#process-count-for-limit');
  assert(processCountForLimit);
  processCountForLimit.innerText = String(info.rendererProcessCountForLimit);

  const processLimit = document.querySelector<HTMLElement>('#process-limit');
  assert(processLimit);
  processLimit.innerText = String(info.rendererProcessLimit);

  const overProcessLimit =
      document.querySelector<HTMLElement>('#over-process-limit');
  assert(overProcessLimit);
  overProcessLimit.innerText =
      (info.rendererProcessCountForLimit >= info.rendererProcessLimit) ? 'Yes' :
                                                                         'No';
}

/**
 * Root of the WebContents tree.
 */
let treeViewRoot: CrTreeElement|null = null;

/**
 * Accumulators for tracking frame and process counts. Reset in
 * loadWebContentsInfo.
 */
let totalFrameCount: number = 0;
let totalCrossProcessFrameCount: number = 0;
let processIdSet: Set<number> = new Set();

/**
 * Initialize and return |treeViewRoot|.
 */
function getTreeViewRoot(): CrTreeElement {
  if (!treeViewRoot) {
    treeViewRoot = document.querySelector('cr-tree');
    assert(treeViewRoot);
    treeViewRoot.detail = {payload: {}, children: {}};
  }
  return treeViewRoot;
}

/**
 * Initialize and return a tree item representing a FrameInfo object and
 * recursively creates its subframe objects. For subframes, pass the parent
 * frame's process ID in `parentProcessId`.
 */
function frameToTreeItem(frame: FrameInfo, parentProcessId: number = -1):
    {item: CrTreeItemElement, count: number} {
  // Count out-of-process iframes.
  const isCrossProcessFrame: boolean =
      parentProcessId !== -1 && parentProcessId !== frame.processId;
  if (isCrossProcessFrame) {
    totalCrossProcessFrameCount++;
  }
  processIdSet.add(frame.processId);

  // Compose the string which will appear in the entry for this frame.
  const prefix = isCrossProcessFrame ? 'OOPIF' : 'Frame';
  let itemLabel = `${prefix}[${frame.processId}:${frame.routingId}]:`;
  if (frame.type === FrameInfo_Type.kBackForwardCache) {
    itemLabel += ` bfcached`;
  } else if (frame.type === FrameInfo_Type.kPrerender) {
    itemLabel += ` prerender`;
  }

  itemLabel += ` SI:${frame.siteInstance.id}`;
  itemLabel += `, SIG:${frame.siteInstance.siteInstanceGroupId}`;
  itemLabel += `, BI:${frame.siteInstance.browsingInstanceId}`;
  if (frame.siteInstance.locked) {
    itemLabel += ', locked';
  } else {
    itemLabel += ', unlocked';
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
  if (frame.siteInstance.isSandboxForIframes) {
    itemLabel += ', iframe-sandbox';
  }
  if (frame.siteInstance.isGuest) {
    itemLabel += ', guest';
  }
  if (frame.siteInstance.isPdf) {
    itemLabel += ', pdf';
  }
  if (frame.siteInstance.storagePartition) {
    itemLabel += `, partition:${frame.siteInstance.storagePartition}`;
  }
  if (frame.lastCommittedUrl) {
    itemLabel += ` | url: ${frame.lastCommittedUrl.url}`;
  }

  const item = document.createElement('cr-tree-item');
  item.label = itemLabel;
  item.detail = {payload: {}, children: {}};
  item.toggleAttribute(MAY_HAVE_CHILDREN_ATTR, true);
  item.expanded = true;

  let frameCount = 1;
  for (const subframe of frame.subframes) {
    const result = frameToTreeItem(subframe, frame.processId);
    const subItem = result.item;
    const count = result.count;

    frameCount += count;
    item.add(subItem);
  }

  return {item: item, count: frameCount};
}

/**
 * Initialize and return a tree item representing the WebContentsInfo object
 * and contains all frames in it as a subtree.
 */
function webContentsToTreeItem(webContents: WebContentsInfo):
    CrTreeItemElement {
  let itemLabel = 'WebContents: ';
  if (webContents.title.length > 0) {
    itemLabel += webContents.title + ', ';
  }

  const item = document.createElement('cr-tree-item');
  item.label = itemLabel;
  item.detail = {payload: {}, children: {}};
  item.toggleAttribute(MAY_HAVE_CHILDREN_ATTR, true);
  item.expanded = true;

  const result = frameToTreeItem(webContents.rootFrame);
  const rootItem = result.item;
  const activeCount = result.count;
  item.add(rootItem);

  // Add data for all root nodes retrieved from back-forward cache.
  let cachedCount = 0;
  for (const cachedRoot of webContents.bfcachedRootFrames) {
    const cachedResult = frameToTreeItem(cachedRoot);
    item.add(cachedResult.item);
    cachedCount++;
  }

  // Add data for all root nodes in prerendered pages.
  let prerenderCount = 0;
  for (const cachedRoot of webContents.prerenderRootFrames) {
    const cachedResult = frameToTreeItem(cachedRoot);
    item.add(cachedResult.item);
    prerenderCount++;
  }

  // Builds a string according to English pluralization rules:
  // buildCountString(0, 'frame') => "0 frames"
  // buildCountString(1, 'frame') => "1 frame"
  // buildCountString(2, 'frame') => "2 frames"
  const buildCountString = ((count: number, name: string) => {
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

  totalFrameCount += activeCount + cachedCount + prerenderCount;

  return item;
}

/**
 * This is a callback which is invoked when the data for WebContents
 * associated with the browser profile is received from the browser process.
 */
function populateWebContentsTab(infos: WebContentsInfo[]) {
  const tree = getTreeViewRoot();

  // Clear the tree first before populating it with the new content.
  tree.items.forEach(item => tree.removeTreeItem(item));

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
  // Reset frame counts.
  totalFrameCount = 0;
  totalCrossProcessFrameCount = 0;
  processIdSet = new Set();

  assert(pageHandler);
  const {infos} = await pageHandler.getAllWebContentsInfo();
  populateWebContentsTab(infos);

  // Post tab, frame, and process counts.
  const tabCount = document.querySelector<HTMLElement>('#tab-count');
  assert(tabCount);
  tabCount.innerText = String(infos.length);
  const frameCount = document.querySelector<HTMLElement>('#frame-count');
  assert(frameCount);
  frameCount.innerText = String(totalFrameCount);
  const oopifCount = document.querySelector<HTMLElement>('#oopif-count');
  assert(oopifCount);
  oopifCount.innerText = String(totalCrossProcessFrameCount);
  const processCount =
      document.querySelector<HTMLElement>('#profile-process-count');
  assert(processCount);
  processCount.innerText = String(processIdSet.size);
}

/**
 * Function which retrieves the currently active isolated origins and inserts
 * them into the page.  It organizes these origins into two lists: persisted
 * isolated origins, which are triggered by password entry and apply only
 * within the current profile, and global isolated origins, which apply to all
 * profiles.
 */
function loadIsolatedOriginInfo() {
  assert(pageHandler);
  // Retrieve any persistent isolated origins for the current profile. Insert
  // them into a list on the page if there is at least one such origin.
  pageHandler.getUserTriggeredIsolatedOrigins().then((response) => {
    const originCount = response.isolatedOrigins.length;
    if (!originCount) {
      return;
    }

    const userOrigins =
        document.querySelector<HTMLElement>('#user-triggered-isolated-origins');
    assert(userOrigins);
    userOrigins.textContent =
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

    userOrigins.appendChild(list);
  });

  pageHandler.getWebTriggeredIsolatedOrigins().then((response) => {
    const originCount = response.isolatedOrigins.length;
    if (!originCount) {
      return;
    }

    const webOrigins =
        document.querySelector<HTMLElement>('#web-triggered-isolated-origins');
    assert(webOrigins);
    webOrigins.textContent =
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

    webOrigins.appendChild(list);
  });

  // Retrieve global isolated origins and insert them into a separate list if
  // there is at least one such origin.  Since these origins may come from
  // multiple sources, include the source info for each origin in parens.
  pageHandler.getGloballyIsolatedOrigins().then((response) => {
    const originCount = response.isolatedOrigins.length;
    if (!originCount) {
      return;
    }

    const globalOrigins =
        document.querySelector<HTMLElement>('#global-isolated-origins');
    assert(globalOrigins);
    globalOrigins.textContent =
        'The following origins are isolated by default for all users (' +
        originCount + ' total).  A description of how each origin was ' +
        ' activated is provided in parentheses.';

    const list = document.createElement('ul');
    for (const originInfo of response.isolatedOrigins) {
      const item = document.createElement('li');
      item.textContent = `${originInfo.origin} (${originInfo.source})`;
      list.appendChild(item);
    }
    globalOrigins.appendChild(list);
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Set up Mojo interface to the backend.
  pageHandler = ProcessInternalsHandler.getRemote();
  assert(pageHandler);

  // Set up the tabbed UI.
  setupTabs();

  // Populate the process count and limit info.
  loadProcessCountInfo();

  const refreshProcessInfoButton =
      document.querySelector<HTMLElement>('#refresh-process-info');
  assert(refreshProcessInfoButton);
  refreshProcessInfoButton.addEventListener('click', loadProcessCountInfo);

  // Get the ProcessPerSite mode and populate it.
  pageHandler.getProcessPerSiteMode().then((response) => {
    const sharingMode =
        document.querySelector<HTMLElement>('#process-per-site-mode');
    assert(sharingMode);
    sharingMode.innerText = response.mode;
  });

  // Get the Site Isolation mode and populate it.
  pageHandler.getIsolationMode().then((response) => {
    const isolationMode =
        document.querySelector<HTMLElement>('#isolation-mode');
    assert(isolationMode);
    isolationMode.innerText = response.mode;
  });
  loadIsolatedOriginInfo();

  // Start loading the information about WebContents.
  loadWebContentsInfo();

  const refreshFrameTreesButton =
      document.querySelector<HTMLElement>('#refresh-frame-trees');
  assert(refreshFrameTreesButton);
  refreshFrameTreesButton.addEventListener('click', loadWebContentsInfo);
});
