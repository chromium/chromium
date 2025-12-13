// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClientRenderer} from './client_renderer.js';
import {initialize} from './main.js';
import {Manager} from './manager.js';

initialize(new Manager(new ClientRenderer()));

const toolbar = document.getElementById('right-pane-toolbar');
const panels = document.getElementById('right-pane-content');

toolbar.addEventListener('click', (event) => {
  if (event.target.classList.contains('toolbar-button')) {
    const selectedButton = toolbar.querySelector('.selected');
    if (selectedButton) {
      selectedButton.classList.remove('selected');
    }
    event.target.classList.add('selected');

    const panelId = event.target.id.replace('-tab-button', '-panel');
    const activePanel = panels.querySelector('.panel.active');
    if (activePanel) {
      activePanel.classList.remove('active');
      activePanel.hidden = true;
    }
    const newActivePanel = panels.querySelector(`#${panelId}`);
    if (newActivePanel) {
      newActivePanel.classList.add('active');
      newActivePanel.hidden = false;
    }

    if (event.target.id === 'players-tab-button') {
      document.getElementById('left-pane').style.display = 'flex';
      document.body.classList.add('players-tab-active');
    } else {
      document.getElementById('left-pane').style.display = 'none';
      document.body.classList.remove('players-tab-active');
    }
  }
});

// Default to the players tab.
let tabButton = document.getElementById('players-tab-button');

const hash = window.location.hash.substring(1);
if (hash) {
  const button = document.getElementById(hash + '-tab-button');
  if (button) {
    tabButton = button;
  }
}
tabButton.click();

window.addEventListener('hashchange', () => {
  const hash = window.location.hash.substring(1);
  if (hash) {
    const button = document.getElementById(hash + '-tab-button');
    if (button) {
      button.click();
    }
  }
});

const contentContainer = document.getElementById('content-container');
const leftPane = document.getElementById('left-pane');
const rightPane = document.getElementById('right-pane');

function updateLayoutForMobile() {
  if (window.innerWidth <= 768) {
    // Move left-pane below the toolbar in right-pane
    if (leftPane.parentElement !== contentContainer) {
      contentContainer.insertBefore(leftPane, rightPane);
    }
  } else {
    // Move left-pane back to the main container
    if (leftPane.parentElement !== contentContainer) {
      contentContainer.insertBefore(leftPane, rightPane);
    }
  }
}

window.addEventListener('resize', updateLayoutForMobile);
updateLayoutForMobile();
