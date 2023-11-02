// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var examples = [
  {name: 'bullet', text: 'Bullet Physics'},
  {name: 'earth', text: 'Raycasted Earth'},
  {name: 'lua', text: 'Lua Interpreter'},
  {name: 'life', text: 'Game of Life'},
  {name: 'voronoi', text: 'Voronoi Simulation'},
  {name: 'smoothlife', text: 'SmoothLife'},
  {name: 'cube', text: 'Rotating Cube'},
];
var exampleMap = {};  // Created below.

var isChrome = /Chrome\/([^\s]+)/.test(navigator.userAgent);
var isMobile = /Mobi/.test(navigator.userAgent);
var hasPnacl = navigator.mimeTypes['application/x-pnacl'] !== undefined;

if (isChrome && !isMobile) {
  if (hasPnacl) {
    makeExampleList();
    if (history.state == null) {
      updateViewFromLocation();
    }

    window.onpopstate = function(event) {
      var exampleName = event.state;
      loadExample(exampleName);
    }
  } else {
    // Older version of Chrome?
    showOldChromeErrorMessage();
  }
} else {
  // Not Chrome, or is mobile Chrome.
  showNotChromeErrorMessage();
}

function makeExampleList() {
  var listEl = document.querySelector('nav ul');
  for (var i = 0; i < examples.length; ++i) {
    var example = examples[i];

    // Populate exampleMap
    exampleMap[example.name] = example;

    // Create li
    var listItemEl = document.createElement('li');
    var anchorEl = document.createElement('a');
    listItemEl.setAttribute('id', example.name);
    anchorEl.setAttribute('href', getExampleUrl(example.name));
    anchorEl.setAttribute('target', 'content');
    anchorEl.textContent = example.text;

    // Listen for clicks and update the nav
    anchorEl.addEventListener('click', onLinkClick.bind(example), false);

    listItemEl.appendChild(anchorEl);
    listEl.appendChild(listItemEl);
  }
}

function getExampleUrl(exampleName) {
  return '/static/' + exampleName + '/index.html';
}

function onLinkClick(evt) {
  evt.preventDefault();
  pushState(this.name);
  loadExample(this.name);
}

function updateViewFromLocation() {
  // Get the location's path.
  var anchorEl = document.createElement('a');
  anchorEl.href = location.href;
  var pathname = anchorEl.pathname;

  // Load the correct page, based on the demo name.
  var matches = pathname.match(/demo(?:\/(.*))?$/);
  var iframeUrl = null;
  if (matches) {
    var matchedExample = matches[1];
    // Strip trailing slash, if any.
    if (matchedExample && matchedExample.slice(-1) === '/') {
      matchedExample = matchedExample.slice(0, -1);
    }

    if (matchedExample in exampleMap) {
      replaceState(matchedExample);
      loadExample(matchedExample);
    } else {
      replaceHomeState();
      createHomeIframe();
    }
  }
}

function loadExample(exampleName) {
  updateTitle(exampleName);
  createExampleIframe(exampleName);
  updateNav(exampleName);
}

function updateNav(exampleName) {
  var links = document.querySelectorAll('li a');
  for (var l = 0; l < links.length; l++) {
    links[l].classList.remove('active');
  }

  if (exampleName != 'home')
    document.querySelector('li#' + exampleName + ' a').classList.add('active');
}

function updateTitle(exampleName) {
  var title = 'PNaCl Demos';
  if (exampleName != 'home') {
    title += ': ' + exampleMap[exampleName].text;
  }
  document.title = title;
}

function createExampleIframe(exampleName) {
  createIframe(getExampleUrl(exampleName));
}

function createHomeIframe() {
  createIframe('/static/home/index.html');
}

function createIframe(src) {
  var iframeEl = document.querySelector('iframe');
  if (iframeEl === null) {
    iframeEl = document.createElement('iframe');
    iframeEl.setAttribute('frameborder', '0');
    iframeEl.setAttribute('width', '100%');
    iframeEl.setAttribute('height', '100%');
    iframeEl.src = src;
    document.querySelector('section').appendChild(iframeEl);
  } else {
    iframeEl.contentDocument.location.replace(src);
  }
}

function pushState(exampleName) {
  window.history.pushState(exampleName, '', '/demo/' + exampleName);
}

function replaceState(exampleName) {
  window.history.replaceState(exampleName, '', '/demo/' + exampleName);
}

function replaceHomeState() {
  window.history.replaceState('home', '', '/demo');
}

function showOldChromeErrorMessage() {
  document.getElementById('old-chrome').removeAttribute('hidden');
}

function showNotChromeErrorMessage() {
  document.getElementById('not-chrome').removeAttribute('hidden');
}
