// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('appcache', function() {
  'use strict';

  var VIEW_DETAILS_TEXT = 'View Details';
  var HIDE_DETAILS_TEXT = 'Hide Details';
  var GET_ALL_APPCACHE = 'getAllAppCache';
  var DELETE_APPCACHE = 'deleteAppCache';
  var GET_APPCACHE_DETAILS = 'getAppCacheDetails';
  var GET_FILE_DETAILS = 'getFileDetails';

  var manifestsToView = [];
  var manifestsToDelete = [];
  var fileDetailsRequests = [];

  function Manifest(url, path, link) {
    this.url = url;
    this.path = path;
    this.link = link;
  }

  function FileRequest(fileURL, manifestURL, path, groupId, responseId) {
    this.fileURL = fileURL;
    this.partitionPath = path;
    this.manifestURL = manifestURL;
    this.groupId = groupId;
    this.responseId = responseId;
  }

  function getFirstAncestor(node, selector) {
    while (node) {
      if (selector(node)) {
        break;
      }
      node = node.parentNode;
    }
    return node;
  }

  function getItemByProperties(list, properties, values) {
    return list.find(function(candidate) {
      return properties.every(function(key, i) {
        return candidate[key] == values[i];
      });
    }) ||
        null;
  }

  function removeFromList(list, item, properties) {
    var pos = 0;
    while (pos < list.length) {
      var candidate = list[pos];
      if (properties.every(function(key) {
            return candidate[key] == item[key];
          })) {
        list.splice(pos, 1);
      } else {
        pos++;
      }
    }
  }

  function initialize() {
    chrome.send(GET_ALL_APPCACHE);
  }


  function onAllAppCacheInfoReady(partition_path, data) {
    var template = jstGetTemplate('appcache-list-template');
    var container = $('appcache-list');
    container.appendChild(template);
    jstProcess(
        new JsEvalContext(
            {appcache_vector: data, partition_path: partition_path}),
        template);
    var removeLinks = container.querySelectorAll('a.remove-manifest');
    for (var i = 0; i < removeLinks.length; ++i) {
      removeLinks[i].onclick = deleteAppCacheInfoEventHandler;
    }
    var viewLinks = container.querySelectorAll('a.view-details');
    for (i = 0; i < viewLinks.length; ++i) {
      viewLinks[i].onclick = getAppCacheInfoEventHandler;
    }
  }

  function getAppCacheInfoEventHandler(event) {
    var link = event.target;
    if (link.text.indexOf(VIEW_DETAILS_TEXT) === -1) {
      hideAppCacheInfo(link);
    } else {
      var manifestURL = getFirstAncestor(link, function(node) {
                          return !!node.manifestURL;
                        }).manifestURL;
      var partitionPath = getFirstAncestor(link, function(node) {
                            return !!node.partitionPath;
                          }).partitionPath;
      var manifest = new Manifest(manifestURL, partitionPath, link);
      if (getItemByProperties(
              manifestsToView, ['url', 'path'], [manifestURL, partitionPath])) {
        return;
      }
      manifestsToView.push(manifest);
      chrome.send(GET_APPCACHE_DETAILS, [partitionPath, manifestURL]);
    }
  }

  function hideAppCacheInfo(link) {
    getFirstAncestor(link, function(node) {
      return node.className === 'appcache-info-item';
    }).querySelector('.appcache-details').innerHTML = '';
    link.text = VIEW_DETAILS_TEXT;
  }

  function onAppCacheDetailsReady(manifestURL, partitionPath, details) {
    if (!details) {
      console.log(
          'Cannot show details for "' + manifestURL + '" on partition ' +
          '"' + partitionPath + '".');
      return;
    }
    var manifest = getItemByProperties(
        manifestsToView, ['url', 'path'], [manifestURL, partitionPath]);
    var link = manifest.link;
    removeFromList(manifestsToView, manifest, ['url', 'path']);
    var container = getFirstAncestor(link, function(node) {
                      return node.className === 'appcache-info-item';
                    }).querySelector('.appcache-details');
    var template = jstGetTemplate('appcache-info-template');
    container.appendChild(template);
    jstProcess(
        new JsEvalContext({items: simplifyAppCacheInfo(details)}), template);
    var fileLinks =
        container.querySelectorAll('a.appcache-info-template-file-url');
    for (var i = 0; i < fileLinks.length; ++i) {
      fileLinks[i].onclick = getFileContentsEventHandler;
    }
    link.text = HIDE_DETAILS_TEXT;
  }

  function simplifyAppCacheInfo(detailsVector) {
    var simpleVector = [];
    for (var index = 0; index < detailsVector.length; ++index) {
      var details = detailsVector[index];
      var properties = [];
      if (details.isManifest) {
        properties.push('Manifest');
      }
      if (details.isExplicit) {
        properties.push('Explicit');
      }
      if (details.isMaster) {
        properties.push('Master');
      }
      if (details.isForeign) {
        properties.push('Foreign');
      }
      if (details.isIntercept) {
        properties.push('Intercept');
      }
      if (details.isFallback) {
        properties.push('Fallback');
      }
      properties = properties.join(',');
      simpleVector.push({
        size: details.size,
        properties: properties,
        fileUrl: details.url,
        responseId: details.responseId
      });
    }
    return simpleVector;
  }

  function deleteAppCacheInfoEventHandler(event) {
    var link = event.target;
    var manifestURL = getFirstAncestor(link, function(node) {
                        return !!node.manifestURL;
                      }).manifestURL;
    var partitionPath = getFirstAncestor(link, function(node) {
                          return !!node.partitionPath;
                        }).partitionPath;
    var manifest = new Manifest(manifestURL, partitionPath, link);
    manifestsToDelete.push(manifest);
    chrome.send(DELETE_APPCACHE, [partitionPath, manifestURL]);
  }

  function onAppCacheInfoDeleted(partitionPath, manifestURL, deleted) {
    var manifest = getItemByProperties(
        manifestsToDelete, ['url', 'path'], [manifestURL, partitionPath]);
    if (manifest && deleted) {
      var link = manifest.link;
      var appcacheItemNode = getFirstAncestor(link, function(node) {
        return node.className === 'appcache-info-item';
      });
      var appcacheNode = getFirstAncestor(link, function(node) {
        return node.className === 'appcache-item';
      });
      appcacheItemNode.parentNode.removeChild(appcacheItemNode);
      if (appcacheNode.querySelectorAll('.appcache-info-item').length === 0) {
        appcacheNode.parentNode.removeChild(appcacheNode);
      }
    } else if (!deleted) {
      // For some reason, the delete command failed.
      console.log(
          'Manifest "' + manifestURL + '" on partition "' + partitionPath +
          ' cannot be accessed.');
    }
  }

  function getFileContentsEventHandler(event) {
    var link = event.target;
    var partitionPath = getFirstAncestor(link, function(node) {
                          return !!node.partitionPath;
                        }).partitionPath;
    var manifestURL = getFirstAncestor(link, function(node) {
                        return !!node.manifestURL;
                      }).manifestURL;
    var groupId = getFirstAncestor(link, function(node) {
                    return !!node.groupId;
                  }).groupId;
    var responseId = link.responseId;

    if (!getItemByProperties(
            fileDetailsRequests, ['manifestURL', 'groupId', 'responseId'],
            [manifestURL, groupId, responseId])) {
      var fileRequest = new FileRequest(
          link.innerText, manifestURL, partitionPath, groupId, responseId);
      fileDetailsRequests.push(fileRequest);
      chrome.send(
          GET_FILE_DETAILS, [partitionPath, manifestURL, groupId, responseId]);
    }
  }

  function onFileDetailsFailed(response, code) {
    var request = getItemByProperties(
        fileDetailsRequests, ['manifestURL', 'groupId', 'responseId'],
        [response.manifestURL, response.groupId, response.responseId]);
    console.log(
        'Failed to get file information for file "' + request.fileURL +
        '" from partition "' + request.partitionPath +
        '" (net result code:' + code + ').');
    removeFromList(
        fileDetailsRequests, request, ['manifestURL', 'groupId', 'responseId']);
  }

  function onFileDetailsReady(response, headers, raw_data) {
    var request = getItemByProperties(
        fileDetailsRequests, ['manifestURL', 'groupId', 'responseId'],
        [response.manifestURL, response.groupId, response.responseId]);
    removeFromList(
        fileDetailsRequests, request, ['manifestURL', 'groupId', 'responseId']);
    var doc = window.open().document;
    var head = document.createElement('head');
    doc.title = 'File Details: '.concat(request.fileURL);
    var headersDiv = doc.createElement('div');
    headersDiv.innerHTML = headers;
    doc.body.appendChild(headersDiv);
    var hexDumpDiv = doc.createElement('div');
    hexDumpDiv.innerHTML = raw_data;
    var linkToManifest = doc.createElement('a');
    linkToManifest.style.color = '#3C66DD';
    linkToManifest.href = request.fileURL;
    linkToManifest.target = '_blank';
    linkToManifest.innerHTML = request.fileURL;

    doc.body.appendChild(linkToManifest);
    doc.body.appendChild(headersDiv);
    doc.body.appendChild(hexDumpDiv);

    copyStylesFrom(document, doc);
  }

  function copyStylesFrom(src, dest) {
    var styles = src.querySelector('style');
    if (dest.getElementsByTagName('style').length < 1) {
      dest.head.appendChild(dest.createElement('style'));
    }
    var destStyle = dest.querySelector('style');
    var tmp = '';
    for (var i = 0; i < styles.length; ++i) {
      tmp += styles[i].innerHTML;
    }
    destStyle.innerHTML = tmp;
  }

  return {
    initialize: initialize,
    onAllAppCacheInfoReady: onAllAppCacheInfoReady,
    onAppCacheInfoDeleted: onAppCacheInfoDeleted,
    onAppCacheDetailsReady: onAppCacheDetailsReady,
    onFileDetailsReady: onFileDetailsReady,
    onFileDetailsFailed: onFileDetailsFailed
  };
});

document.addEventListener('DOMContentLoaded', appcache.initialize);
