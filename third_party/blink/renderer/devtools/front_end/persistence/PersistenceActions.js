// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Persistence.PersistenceActions = {};

/**
 * @implements {UI.ContextMenu.Provider}
 * @unrestricted
 */
Persistence.PersistenceActions.ContextMenuProvider = class {
  /**
   * @override
   * @param {!Event} event
   * @param {!UI.ContextMenu} contextMenu
   * @param {!Object} target
   */
  appendApplicableItems(event, contextMenu, target) {
    const contentProvider = /** @type {!Common.ContentProvider} */ (target);
    async function saveAs() {
      if (contentProvider instanceof Workspace.UISourceCode)
        /** @type {!Workspace.UISourceCode} */ (contentProvider).commitWorkingCopy();
      let content = await contentProvider.requestContent();
      if (await contentProvider.contentEncoded())
        content = window.atob(content);
      const url = contentProvider.contentURL();
      Workspace.fileManager.save(url, /** @type {string} */ (content), true);
      Workspace.fileManager.close(url);
    }

    if (contentProvider.contentType().isDocumentOrScriptOrStyleSheet())
      contextMenu.saveSection().appendItem(Common.UIString('Save as...'), saveAs);

    // Retrieve uiSourceCode by URL to pick network resources everywhere.
    const uiSourceCode = Workspace.workspace.uiSourceCodeForURL(contentProvider.contentURL());
    if (uiSourceCode && Persistence.networkPersistenceManager.canSaveUISourceCodeForOverrides(uiSourceCode)) {
      contextMenu.saveSection().appendItem(Common.UIString('Save for overrides'), () => {
        uiSourceCode.commitWorkingCopy();
        Persistence.networkPersistenceManager.saveUISourceCodeForOverrides(
            /** @type {!Workspace.UISourceCode} */ (uiSourceCode));
        Common.Revealer.reveal(uiSourceCode);
      });
    }

    const binding = uiSourceCode && Persistence.persistence.binding(uiSourceCode);
    const fileURL = binding ? binding.fileSystem.contentURL() : contentProvider.contentURL();
    if (fileURL.startsWith('file://')) {
      const path = Common.ParsedURL.urlToPlatformPath(fileURL, Host.isWin());
      contextMenu.revealSection().appendItem(
          Common.UIString('Open in containing folder'), () => InspectorFrontendHost.showItemInFolder(path));
    }
  }
};
