// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Returns a display string given the date & time specified in dateString.
// Example format: 2023-04-30T14:20:10
function getDateTimeDisplayString(dateString: string|undefined): string {
  if (!dateString || dateString.length == 0) {
    return '';
  }
  const date = new Date(dateString);
  return date.toLocaleString(
      'default', {year: 'numeric', day: 'numeric', month: 'short'});
}

// Returns a string representation of the size sizeInBytes.
function getSizeDisplayString(sizeInBytes: number|undefined): string {
  if (!sizeInBytes || sizeInBytes == 0) {
    return '0 B';
  }
  if (sizeInBytes < 1024) {
    return sizeInBytes.toFixed() + ' B';
  }
  if (sizeInBytes < (1024 * 1024)) {
    return (sizeInBytes / 1024).toFixed(1) + ' KB';
  }
  if (sizeInBytes < (1024 * 1024 * 1024)) {
    return (sizeInBytes / 1024 / 1024).toFixed(1) + ' MB';
  }

  return (sizeInBytes / 1024 / 1024 / 1024).toFixed(1) + ' GB';
}

// A set of common audio file extensions.
const AUDIO_FORMATS =
    new Set(['AAC', 'AIFF', 'ALAC', 'DSD', 'FLAC', 'MP3', 'OGG', 'WAV']);
// A set of common image file extensions.
const IMAGE_FORMATS =
    new Set(['BMP', 'GIF', 'JPEG', 'JPG', 'PNG', 'TIF', 'TIFF']);
// A set of common video file extensions.
const VIDEO_FORMATS = new Set([
  'AVCHD', 'AVI', 'FLV', 'M4P', 'M4V', 'MOV', 'MP2', 'MP4', 'MPE', 'MPEG',
  'MPG', 'MPV', 'OGG', 'QT', 'SWF', 'WEBM', 'WMV'
]);

// Returns an icon (as a single emoji item) based on the given `filename`'s
// extension.
function iconForFilename(filename: string): string {
  let extension = filename.split('.').pop();
  if (extension) {
    extension = extension.toUpperCase();
  }

  if (!extension) {
    return '📄';
  }

  if (extension == 'PDF') {
    return '📋';
  }

  if (AUDIO_FORMATS.has(extension)) {
    return '🎶';
  }

  if (IMAGE_FORMATS.has(extension)) {
    return '📷';
  }

  if (VIDEO_FORMATS.has(extension)) {
    return '📹';
  }

  return '📄';
}

declare interface Item {
  name: string;
  size?: number;

  accessed?: string;
  created?: string;
  modified?: string;

  excludedFromBackups?: boolean;

  contents?: Item[]
}

// Returns a sorted list of the given `items` based on the value of `sorting`.
function sortItems(items: Array<Item>, sorting: string): Array<Item> {
  const sortedItems = items;
  // return items.toSorted((a: Item, b: Item) => {
  sortedItems.sort((a: Item, b: Item) => {
    switch (sorting) {
      case 'nameAsc':
        return a.name.localeCompare(b.name);
      case 'nameDesc':
        return b.name.localeCompare(a.name);
      case 'sizeAsc':
        if (!a.size) {
          return -1;
        }
        if (!b.size) {
          return 1;
        }
        if (a.size < b.size) {
          return -1;
        } else if (a.size == b.size) {
          return 0;
        }
        return 1;
      case 'sizeDesc':
        if (!b.size) {
          return -1;
        }
        if (!a.size) {
          return 1;
        }
        if (b.size < a.size) {
          return -1;
        } else if (a.size == b.size) {
          return 0;
        }
        return 1;
      case 'accessedAsc':
        if (!b.accessed) {
          return -1;
        }
        if (!a.accessed) {
          return 1;
        }
        return b.accessed.localeCompare(a.accessed);
      case 'accessedDesc':
        if (!a.accessed) {
          return -1;
        }
        if (!b.accessed) {
          return 1;
        }
        return a.accessed.localeCompare(b.accessed);
    }
    return 0;
  });
  return sortedItems;
}

let collapsedDirectoryPaths: Set<string> = new Set();

// Updates the expanded/collapsed state of directory contents and updates
// directory icons to be in the correct open/closed state.
function refreshExpandedState(): void {
  const contents = document.getElementById('contents')!;
  for (const row of contents.querySelectorAll('.item_row')) {
    if (row.hasAttribute('path')) {
      const rowPath = row.getAttribute('path')!;

      if (row.classList.contains('directory')) {
        const itemIcon = row.querySelector('.item_icon') as HTMLElement;
        if (collapsedDirectoryPaths.has(rowPath)) {
          itemIcon.innerText = '📁';
        } else {
          itemIcon.innerText = '📂';
        }
      }

      let collapsed = false;
      for (const collapsedPath of collapsedDirectoryPaths) {
        if (rowPath.startsWith(collapsedPath + '/')) {
          collapsed = true;
          break;
        }
      }

      if (collapsed) {
        (row as HTMLElement).style.display = 'none';
      } else {
        (row as HTMLElement).style.display = 'flex';
      }
    }
  }
}

// Creates row items for `root` and all children, recursively.
function createEntryRowForRoot(root: Item, level = 0, parentPath = ''): void {
  const path = parentPath + '/' + root.name;

  let currentRootIncludesThisRow = true;
  if (window.location.hash) {
    const rootPath = decodeURIComponent(window.location.hash.substring(1))
    currentRootIncludesThisRow = path.indexOf(rootPath) == 0;
  }

  let nextLevel = level;
  if (currentRootIncludesThisRow &&
      // No search terms or this item matches the search terms.
      (!searchTerms ||
       root.name.toUpperCase().indexOf(searchTerms.toUpperCase()) >= 0)) {
    nextLevel = nextLevel + 1;

    const itemRow = document.createElement('div');
    itemRow.setAttribute('path', path);
    if (root.contents) {
      itemRow.classList.add('directory');
    }
    itemRow.classList.add('item_row');

    const itemInset = document.createElement('span');
    itemInset.classList.add('item_spacing');
    itemInset.style.width = (25 * level) + 'px';
    itemRow.appendChild(itemInset);

    const itemIcon = document.createElement('span');
    itemIcon.classList.add('item_icon');
    if (!root.contents) {
      itemIcon.innerText = iconForFilename(root.name);
    }
    itemRow.appendChild(itemIcon);

    const itemName = document.createElement('span');
    itemName.classList.add('item_name');
    let backupIcon = '<span class="backed_up_cloud">☁️</span>';
    if (root.excludedFromBackups) {
      backupIcon = '';
    }
    let makeDirRootLink = '';
    if (root.contents && level > 0) {
      makeDirRootLink =
          '<a class="arrow-up" href="#' + encodeURIComponent(path) + '">⬆️</a>';
    }
    itemName.innerHTML = '' + root.name + backupIcon + makeDirRootLink;
    itemRow.appendChild(itemName);

    const itemSize = document.createElement('span');
    itemSize.classList.add('item_size');
    itemSize.innerText = getSizeDisplayString(root.size);
    itemRow.appendChild(itemSize);

    const itemAccessed = document.createElement('span');
    itemAccessed.classList.add('item_accessed');
    itemAccessed.innerText = getDateTimeDisplayString(root.accessed);
    itemRow.appendChild(itemAccessed);

    const itemCreated = document.createElement('span');
    itemCreated.classList.add('item_created');
    itemCreated.innerText = getDateTimeDisplayString(root.created);
    itemRow.appendChild(itemCreated);

    const itemModified = document.createElement('span');
    itemModified.classList.add('item_modified');
    itemModified.innerText = getDateTimeDisplayString(root.modified);
    itemRow.appendChild(itemModified);

    if (parentPath.split('/').length % 2 == 1) {
      itemName.classList.add('grey_bg');
      itemSize.classList.add('grey_bg');
      itemAccessed.classList.add('grey_bg');
      itemCreated.classList.add('grey_bg');
      itemModified.classList.add('grey_bg');
    }

    const contents = document.getElementById('contents')!;
    contents.appendChild(itemRow);

    if (root.contents) {
      itemRow.addEventListener('click', function(event) {
        if (!event.target || !(event.target instanceof Element) ||
            event.target.classList.contains('arrow-up')) {
          // Don't change expansion state on arrow click.
          return;
        }

        if (collapsedDirectoryPaths.has(path)) {
          // Expand previously collapsed directory.
          collapsedDirectoryPaths.delete(path);
        } else {
          // Collapse previously expanded directory.
          collapsedDirectoryPaths.add(path);
        }

        refreshExpandedState();
      });
    }
  }
  if (root.contents) {
    let sorting = 'nameAsc';
    const sortDropdown = document.getElementById('sorting');
    if (sortDropdown && sortDropdown instanceof HTMLSelectElement) {
      sorting = sortDropdown.value;
    }
    const sortedItems = sortItems(root.contents, sorting);
    for (const item of sortedItems) {
      createEntryRowForRoot(item, nextLevel, path);
    }
  }
}

let allStatistics: Item|null = null;
let searchTerms: string|null = null;
let rootPath: string|null = null;
// Reloads the displayed items, taking into account collapsed directories,
// `searchTerms`, and the chosen sorting.
function reloadStatistics(): void {
  const contents = document.getElementById('contents')!;
  for (const row of contents.querySelectorAll('div:not(.header_row)')) {
    contents.removeChild(row);
  }

  if (window.location.hash) {
    rootPath = decodeURIComponent(window.location.hash.substring(1))
    document.getElementById('root_path')!.innerText = rootPath;

    let one_up_location = '';
    if (rootPath.includes('/')) {
      one_up_location =
          encodeURIComponent(rootPath.substring(0, rootPath.lastIndexOf('/')));
    }

    document.getElementById('nav_up')!.setAttribute(
        'onclick', 'window.location.hash=\'#' + one_up_location + '\'');
  } else {
    document.getElementById('root_path')!.innerText = '/';
  }

  if (!allStatistics) {
    return;
  }
  createEntryRowForRoot(allStatistics);
  refreshExpandedState();
}

// Recursively marks all directories in items as collapsed
function collapseDirectories(items: Array<Item>, parentPath = ''): void {
  if (!items || items.length == 0) {
    return;
  }
  for (const item of items) {
    const path = parentPath + '/' + item.name;
    if (item.contents) {
      let currentRootIncludesThisItemAsChild = true;
      if (window.location.hash) {
        const rootPath = decodeURIComponent(window.location.hash.substring(1))
        if (path == rootPath) {
          // Don't collapse the top level item.
          currentRootIncludesThisItemAsChild = false;
        }
        else {
          currentRootIncludesThisItemAsChild = path.indexOf(rootPath) == 0;
        }
      }

      if (currentRootIncludesThisItemAsChild) {
        collapsedDirectoryPaths.add(path);
      }
      collapseDirectories(item.contents, path);
    }
  }
}

// Marks every directory as collapsed and refreshes the UI.
function collapseAllDirectories(): void {
  if (!allStatistics) {
    return;
  }

  collapsedDirectoryPaths.clear();
  collapseDirectories([allStatistics]);
  refreshExpandedState();
}

// Marks every directory as expanded and refreshes the UI.
function expandAllDirectories(): void {
  collapsedDirectoryPaths.clear();
  refreshExpandedState();
}

// Triggered when the user chose a data file. Reads the file contents and loads
// the contents.
function fileSelected(file: File): void {
  // Clear file selection listeners
  const reportSelector = document.getElementById('report_file_input')!;
  reportSelector.removeEventListener('change', fileInputValueChanged);

  const dropArea = document.getElementById('drop_target')!;
  dropArea.removeEventListener('dragover', dragoverEvent);
  dropArea.removeEventListener('drop', dropEvent);

  document.getElementById('report_upload')!.hidden = true;
  document.getElementById('loading')!.hidden = false;

  document.getElementById('local_file')!.innerText = file.name;

  const fileReader = new FileReader();
  fileReader.addEventListener('load', () => {
    const statistics = JSON.parse(fileReader.result as string);
    document.getElementById('loading')!.hidden = true;
    document.getElementById('viewer')!.hidden = false;

    allStatistics = statistics;
    reloadStatistics();
  });
  fileReader.readAsText(file);
}

function fileInputValueChanged(event: Event) {
  if (!event.target || !(event.target instanceof HTMLInputElement)) {
    return;
  }
  const fileList = event.target.files;
  if (fileList && fileList.length > 0) {
    fileSelected(fileList[0] as File);
  }
}

function dragoverEvent(event: DragEvent) {
  event.stopPropagation();
  event.preventDefault();
  if (!event.dataTransfer) {
    return;
  }
  // Style the drag-and-drop as a "copy file" operation.
  event.dataTransfer.dropEffect = 'copy';
}

function dropEvent(event: DragEvent) {
  event.stopPropagation();
  event.preventDefault();
  if (!event.dataTransfer) {
    return;
  }
  const fileList = event.dataTransfer.files;
  if (fileList && fileList.length > 0) {
    fileSelected(fileList[0] as File);
  }
}

function searchBarTextChanged(event: Event) {
  if (!event.target || !(event.target instanceof HTMLInputElement)) {
    return;
  }

  searchTerms = event.target.value;
  reloadStatistics();
}

document.addEventListener('DOMContentLoaded', function() {
  const reportSelector = document.getElementById('report_file_input')!;
  reportSelector.addEventListener('change', fileInputValueChanged);

  const dropArea = document.getElementById('drop_target')!;
  dropArea.addEventListener('dragover', dragoverEvent);
  dropArea.addEventListener('drop', dropEvent);

  const searchbar = document.getElementById('searchbar')!;
  searchbar.addEventListener('input', searchBarTextChanged);
  window.addEventListener('hashchange', reloadStatistics);

  const sortDropdown = document.getElementById('sorting')!;
  sortDropdown.addEventListener('change', reloadStatistics);
});
