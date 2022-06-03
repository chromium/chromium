// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

var tree;

function prepareWebKitXMLViewer()
{
  var html = createHTMLElement('html');
  var head = createHTMLElement('head');
  html.appendChild(head);
  var style = createHTMLElement('style');
  style.id = 'xml-viewer-style';
  head.appendChild(style);
  var body = createHTMLElement('body');
  html.appendChild(body);
  var sourceXML = createHTMLElement('div');
  sourceXML.id = 'webkit-xml-viewer-source-xml';
  body.appendChild(sourceXML);

  var child;
  while (child = document.firstChild) {
    document.removeChild(child);
    if (child.nodeType != Node.DOCUMENT_TYPE_NODE)
      sourceXML.appendChild(child);
  }
  document.appendChild(html);

  var header = createHTMLElement('div');
  body.appendChild(header);
  header.classList.add('header');
  var headerSpan = createHTMLElement('span');
  header.appendChild(headerSpan);
  headerSpan.textContent =
      'This XML file does not appear to have any style information ' +
      'associated with it. The document tree is shown below.';
  header.appendChild(createHTMLElement('br'));

  tree = createHTMLElement('div');
  body.appendChild(tree);
  tree.classList.add('pretty-print');
  window.onload = sourceXMLLoaded;
}

function sourceXMLLoaded()
{
  var sourceXML = document.getElementById('webkit-xml-viewer-source-xml');
  if (!sourceXML)
    return;  // Stop if some XML tree extension is already processing this
             // document

  for (var child = sourceXML.firstChild; child; child = child.nextSibling)
    processNode(tree, child);

  initButtons();

  return false;
}

// Tree processing.

function processNode(parentElement, node)
{
  switch (node.nodeType) {
    case Node.PROCESSING_INSTRUCTION_NODE:
      processProcessingInstruction(parentElement, node);
      break;
    case Node.ELEMENT_NODE:
      processElement(parentElement, node);
      break;
    case Node.COMMENT_NODE:
      processComment(parentElement, node);
      break;
    case Node.TEXT_NODE:
      processText(parentElement, node);
      break;
    case Node.CDATA_SECTION_NODE:
      processCDATA(parentElement, node);
      break;
    default:
      // No-op for unsupported node types e.g. Node.DOCUMENT_FRAGMENT_NODE.
  }
}

function processElement(parentElement, node)
{
  if (!node.firstChild)
    processEmptyElement(parentElement, node);
  else {
    var child = node.firstChild;
    if (child.nodeType == Node.TEXT_NODE && !child.nextSibling)
      processShortTextOnlyElement(parentElement, node);
    else
      processComplexElement(parentElement, node);
  }
}

function processEmptyElement(parentElement, node)
{
  var line = createLine();
  line.appendChild(createTag(node, false, true));
  parentElement.appendChild(line);
}

function processShortTextOnlyElement(parentElement, node)
{
  var line = createLine();
  line.appendChild(createTag(node, false, false));
  for (var child = node.firstChild; child; child = child.nextSibling)
    line.appendChild(createText(child.nodeValue));
  line.appendChild(createTag(node, true, false));
  parentElement.appendChild(line);
}

function processComplexElement(parentElement, node)
{
  var folder = createFolder();
  folder.start.appendChild(createTag(node, false, false));

  for (var child = node.firstChild; child; child = child.nextSibling)
    processNode(folder.openedContent, child);

  folder.end.appendChild(createTag(node, true, false));

  parentElement.appendChild(folder);
}

function processComment(parentElement, node)
{
  var line = createLine();
  line.appendChild(createComment('<!-- ' + node.nodeValue + ' -->'));
  parentElement.appendChild(line);
}

function processCDATA(parentElement, node)
{
  var line = createLine();
  line.appendChild(createText('<![CDATA[ ' + node.nodeValue + ' ]]>'));
  parentElement.appendChild(line);
}

function processProcessingInstruction(parentElement, node)
{
  var line = createLine();
  line.appendChild(
      createComment('<?' + node.nodeName + ' ' + node.nodeValue + '?>'));
  parentElement.appendChild(line);
}

function processText(parentElement, node)
{
  parentElement.appendChild(createText(node.nodeValue));
}

// Tree rendering.

function createHTMLElement(elementName)
{
  return document.createElementNS('http://www.w3.org/1999/xhtml', elementName)
}

function createFolder()
{
  var folder = createHTMLElement('div');
  folder.classList.add('folder');

  folder.start = createLine();
  folder.start.appendChild(createFolderButton());
  folder.appendChild(folder.start);

  folder.openedContent = createHTMLElement('div');
  folder.openedContent.classList.add('opened');
  folder.appendChild(folder.openedContent);

  // Folded content.
  folder.foldedContent = createText('...');
  folder.foldedContent.classList.add('folded');
  folder.foldedContent.classList.add('hidden');
  folder.appendChild(folder.foldedContent);

  folder.end = createLine();
  folder.appendChild(folder.end);

  return folder;
}

function createFolderButton(str) {
  var button = createHTMLElement('span');
  button.classList.add('folder-button');
  button.classList.add('fold');
  return button;
}

function createComment(commentString)
{
  var comment = createHTMLElement('span');
  comment.classList.add('comment');
  comment.classList.add('html-comment');
  comment.textContent = commentString;
  return comment;
}

function createText(value)
{
  var text = createHTMLElement('span');
  text.textContent = value;
  return text;
}

function createLine()
{
  var line = createHTMLElement('div');
  line.classList.add('line');
  return line;
}

function createTag(node, isClosing, isEmpty)
{
  var tag = createHTMLElement('span');
  tag.classList.add('html-tag');

  var stringBeforeAttrs = '<';
  if (isClosing)
    stringBeforeAttrs += '/';
  stringBeforeAttrs += node.nodeName;
  var textBeforeAttrs = document.createTextNode(stringBeforeAttrs);
  tag.appendChild(textBeforeAttrs);

  if (!isClosing) {
    for (var i = 0; i < node.attributes.length; i++)
      tag.appendChild(createAttribute(node.attributes[i]));
  }

  var stringAfterAttrs = '';
  if (isEmpty)
    stringAfterAttrs += '/';
  stringAfterAttrs += '>';
  var textAfterAttrs = document.createTextNode(stringAfterAttrs);
  tag.appendChild(textAfterAttrs);

  return tag;
}

function createAttribute(attributeNode)
{
  var attribute = createHTMLElement('span');
  attribute.classList.add('html-attribute');

  var attributeName = createHTMLElement('span');
  attributeName.classList.add('html-attribute-name');
  attributeName.textContent = attributeNode.name;

  var textBefore = document.createTextNode(' ');
  var textBetween = document.createTextNode('="');

  var attributeValue = createHTMLElement('span');
  attributeValue.classList.add('html-attribute-value');
  attributeValue.textContent = attributeNode.value;

  var textAfter = document.createTextNode('"');

  attribute.appendChild(textBefore);
  attribute.appendChild(attributeName);
  attribute.appendChild(textBetween);
  attribute.appendChild(attributeValue);
  attribute.appendChild(textAfter);
  return attribute;
}

function toggleFunction(sectionId) {
  return function() {
    var foldedContent = document.querySelector('#' + sectionId + ' > .folded');
    var openedContent = document.querySelector('#' + sectionId + ' > .opened');
    var folderButton =
        document.querySelector('#' + sectionId + ' > .line > .folder-button');

    if (foldedContent) {
      if (foldedContent.className.includes('hidden'))
        foldedContent.className = 'folded';
      else
        foldedContent.className = 'folded hidden';
    }

    if (openedContent) {
      if (openedContent.className.includes('hidden'))
        openedContent.className = 'opened';
      else
        openedContent.className = 'opened hidden';
    }

    if (folderButton) {
      if (folderButton.className.includes('open'))
        folderButton.className = 'folder-button fold';
      else
        folderButton.className = 'folder-button open';
    }
  };
}

function initButtons()
{
  var sections = document.querySelectorAll('.folder');
  for (var i = 0; i < sections.length; i++) {
    var sectionId = 'folder' + i;
    sections[i].id = sectionId;

    var folderButton = sections[i].querySelector('.folder-button');
    folderButton.onclick = toggleFunction(sectionId);
    folderButton.onmousedown = handleButtonMouseDown;
  }
}

function handleButtonMouseDown(e)
{
   // To prevent selection on double click
   e.preventDefault();
}

prepareWebKitXMLViewer();
