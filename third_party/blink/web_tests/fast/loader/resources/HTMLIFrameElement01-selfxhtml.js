/*
Copyright (c) 2001-2005 World Wide Web Consortium,
(Massachusetts Institute of Technology, European Research Consortium
for Informatics and Mathematics, Keio University). All
Rights Reserved. This work is distributed under the W3C(r) Software License [1] in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/

  function assertSize(descr, expected, actual) {
    var actualSize;
    assertNotNull(descr, actual);
    actualSize = actual.length;
    assertEquals(descr, expected, actualSize);
  }

  function assertEqualsAutoCase(context, descr, expected, actual) {
      if (builder.contentType == "text/html") {
          if(context == "attribute") {
              assertEquals(descr, expected.toLowerCase(), actual.toLowerCase());
          } else {
              assertEquals(descr, expected.toUpperCase(), actual);
          }
      } else {
          assertEquals(descr, expected, actual);
      }
  }

  function assertEqualsCollectionAutoCase(context, descr, expected, actual) {
    //
    //  if they aren't the same size, they aren't equal
    assertEquals(descr, expected.length, actual.length);

    //
    //  if there length is the same, then every entry in the expected list
    //     must appear once and only once in the actual list
    var expectedLen = expected.length;
    var expectedValue;
    var actualLen = actual.length;
    var i;
    var j;
    var matches;
    for(i = 0; i < expectedLen; i++) {
        matches = 0;
        expectedValue = expected[i];
        for(j = 0; j < actualLen; j++) {
            if (builder.contentType == "text/html") {
                if (context == "attribute") {
                    if (expectedValue.toLowerCase() == actual[j].toLowerCase()) {
                        matches++;
                    }
                } else {
                    if (expectedValue.toUpperCase() == actual[j]) {
                        matches++;
                    }
                }
            } else {
                if(expectedValue == actual[j]) {
                    matches++;
                }
            }
        }
        if(matches == 0) {
            assert(descr + ": No match found for " + expectedValue,false);
        }
        if(matches > 1) {
            assert(descr + ": Multiple matches found for " + expectedValue, false);
        }
    }
  }

  function assertEqualsCollection(descr, expected, actual) {
    //
    //  if they aren't the same size, they aren't equal
    assertEquals(descr, expected.length, actual.length);
    //
    //  if there length is the same, then every entry in the expected list
    //     must appear once and only once in the actual list
    var expectedLen = expected.length;
    var expectedValue;
    var actualLen = actual.length;
    var i;
    var j;
    var matches;
    for(i = 0; i < expectedLen; i++) {
        matches = 0;
        expectedValue = expected[i];
        for(j = 0; j < actualLen; j++) {
            if(expectedValue == actual[j]) {
                matches++;
            }
        }
        if(matches == 0) {
            assert(descr + ": No match found for " + expectedValue,false);
        }
        if(matches > 1) {
            assert(descr + ": Multiple matches found for " + expectedValue, false);
        }
    }
  }

  function assertEqualsListAutoCase(context, descr, expected, actual) {
    var minLength = expected.length;
    if (actual.length < minLength) {
        minLength = actual.length;
    }
    //
    for(var i = 0; i < minLength; i++) {
        assertEqualsAutoCase(context, descr, expected[i], actual[i]);
    }
    //
    //  if they aren't the same size, they aren't equal
    assertEquals(descr, expected.length, actual.length);
  }

  function assertEqualsList(descr, expected, actual) {
    var minLength = expected.length;
    if (actual.length < minLength) {
        minLength = actual.length;
    }
    //
    for(var i = 0; i < minLength; i++) {
        if(expected[i] != actual[i]) {
            assertEquals(descr, expected[i], actual[i]);
        }
    }
    //
    //  if they aren't the same size, they aren't equal
    assertEquals(descr, expected.length, actual.length);
  }

  function assertInstanceOf(descr, type, obj) {
    if(type == "Attr") {
        assertEquals(descr,2,obj.nodeType);
        var specd = obj.specified;
    }
  }

  function assertSame(descr, expected, actual) {
    if(expected != actual) {
        assertEquals(descr, expected.nodeType, actual.nodeType);
        assertEquals(descr, expected.nodeValue, actual.nodeValue);
    }
  }

  function assertURIEquals(assertID, scheme, path, host, file, name, query, fragment, isAbsolute, actual) {
    //
    //  URI must be non-null
    assertNotNull(assertID, actual);

    var uri = actual;

    var lastPound = actual.lastIndexOf("#");
    var actualFragment = "";
    if(lastPound != -1) {
        //
        //   substring before pound
        //
        uri = actual.substring(0,lastPound);
        actualFragment = actual.substring(lastPound+1);
    }
    if(fragment != null) assertEquals(assertID,fragment, actualFragment);

    var lastQuestion = uri.lastIndexOf("?");
    var actualQuery = "";
    if(lastQuestion != -1) {
        //
        //   substring before pound
        //
        uri = actual.substring(0,lastQuestion);
        actualQuery = actual.substring(lastQuestion+1);
    }
    if(query != null) assertEquals(assertID, query, actualQuery);

    var firstColon = uri.indexOf(":");
    var firstSlash = uri.indexOf("/");
    var actualPath = uri;
    var actualScheme = "";
    if(firstColon != -1 && firstColon < firstSlash) {
        actualScheme = uri.substring(0,firstColon);
        actualPath = uri.substring(firstColon + 1);
    }

    if(scheme != null) {
        assertEquals(assertID, scheme, actualScheme);
    }

    if(path != null) {
        assertEquals(assertID, path, actualPath);
    }

    if(host != null) {
        var actualHost = "";
        if(actualPath.substring(0,2) == "//") {
            var termSlash = actualPath.substring(2).indexOf("/") + 2;
            actualHost = actualPath.substring(0,termSlash);
        }
        assertEquals(assertID, host, actualHost);
    }

    if(file != null || name != null) {
        var actualFile = actualPath;
        var finalSlash = actualPath.lastIndexOf("/");
        if(finalSlash != -1) {
            actualFile = actualPath.substring(finalSlash+1);
        }
        if (file != null) {
            assertEquals(assertID, file, actualFile);
        }
        if (name != null) {
            var actualName = actualFile;
            var finalDot = actualFile.lastIndexOf(".");
            if (finalDot != -1) {
                actualName = actualName.substring(0, finalDot);
            }
            assertEquals(assertID, name, actualName);
        }
    }

    if(isAbsolute != null) {
        assertEquals(assertID, isAbsolute, actualPath.substring(0,1) == "/");
    }
  }

// size() used by assertSize element
function size(collection)
{
  return collection.length;
}

function same(expected, actual)
{
  return expected === actual;
}

function equalsAutoCase(context, expected, actual) {
    if (builder.contentType == "text/html") {
        if (context == "attribute") {
            return expected.toLowerCase() == actual;
        }
        return expected.toUpperCase() == actual;
    }
    return expected == actual;
}

function toLowerArray(src) {
   var newArray = new Array();
   var i;
   for (i = 0; i < src.length; i++) {
      newArray[i] = src[i].toLowerCase();
   }
   return newArray;
}

function createTempURI(scheme) {
   if (scheme == "http") {
         return "http://localhost:8080/webdav/tmp" + Math.floor(Math.random() * 100000) + ".xml";
   }
   return "file:///tmp/domts" + Math.floor(Math.random() * 100000) + ".xml";
}

function EventMonitor() {
  this.atEvents = new Array();
  this.bubbledEvents = new Array();
  this.capturedEvents = new Array();
  this.allEvents = new Array();
}

EventMonitor.prototype.handleEvent = function(evt) {
    switch(evt.eventPhase) {
       case 1:
       monitor.capturedEvents[monitor.capturedEvents.length] = evt;
       break;

       case 2:
       monitor.atEvents[monitor.atEvents.length] = evt;
       break;

       case 3:
       monitor.bubbledEvents[monitor.bubbledEvents.length] = evt;
       break;
    }
    monitor.allEvents[monitor.allEvents.length] = evt;
}

function DOMErrorImpl(err) {
  this.severity = err.severity;
  this.message = err.message;
  this.type = err.type;
  this.relatedException = err.relatedException;
  this.relatedData = err.relatedData;
  this.location = err.location;
}

function DOMErrorMonitor() {
  this.allErrors = new Array();
}

DOMErrorMonitor.prototype.handleError = function(err) {
    errorMonitor.allErrors[errorMonitor.allErrors.length] = new DOMErrorImpl(err);
}

DOMErrorMonitor.prototype.assertLowerSeverity = function(id, severity) {
    var i;
    for (i = 0; i < this.allErrors.length; i++) {
        if (this.allErrors[i].severity >= severity) {
           assertEquals(id, severity - 1, this.allErrors[i].severity);
        }
    }
}

function UserDataNotification(operation, key, data, src, dst) {
    this.operation = operation;
    this.key = key;
    this.data = data;
    this.src = src;
    this.dst = dst;
}

function UserDataMonitor() {
    this.allNotifications = new Array();
}

UserDataMonitor.prototype.handle = function(operation, key, data, src, dst) {
    userDataMonitor.allNotifications[userDataMonitor.allNotifications.length] =
         new UserDataNotification(operation, key, data, src, dst);
}

function HTMLBuilder() {
    this.contentType = "application/xhtml+xml";
    this.supportedContentTypes = [ "application/xhtml+xml" ];

    this.supportsAsyncChange = false;
    this.async = false;
    this.fixedAttributeNames = [
        "validating",  "expandEntityReferences", "coalescing",
        "signed", "hasNullString", "ignoringElementContentWhitespace", "namespaceAware", "ignoringComments", "schemaValidating"];

    this.fixedAttributeValues = [false,  true, false, true, true , false, true, false, false ];
    this.configurableAttributeNames = [ ];
    this.configurableAttributeValues = [ ];
    this.initializationError = null;
    this.initializationFatalError = null;
    this.skipIncompatibleTests = true;
    this.documentURLs = new Array();
    this.documentVarnames = new Array();
}

HTMLBuilder.prototype.hasFeature = function(feature, version) {
    return document.implementation.hasFeature(feature, version);
}

HTMLBuilder.prototype.getImplementation = function() {
  return document.implementation;
}

HTMLBuilder.prototype.preload = function(frame, varname, url) {
  var i;
  this.documentVarnames[this.documentVarnames.length] = varname;
  this.documentURLs[this.documentURLs.length] = url;
  if (this.documentURLs.length > 1) {
     //
     //   if all the urls are not the same
     //
     for (i = 1; i < this.documentURLs.length; i++) {
         if (this.documentURLs[i] != this.documentURLs[0]) {
             throw "Tests with multiple loads of different documents are not currently supported";
         }
     }
  }
  return 1;
}

HTMLBuilder.prototype.cloneNode = function(srcNode, doc) {
   var clone = null;
   switch(srcNode.nodeType) {
      //
      //  element
      case 1:
      clone = doc.createElementNS(srcNode.namespaceURI, srcNode.nodeName);
      var attrs = srcNode.attributes;
      for(var i = 0; i < attrs.length; i++) {
          var srcAttr = attrs.item(i);
          clone.setAttributeNS(srcAttr.namespaceURI, srcAttr.nodeName, srcAttr.nodeValue);
      }
      var srcChild = srcNode.firstChild;
      while(srcChild != null) {
         var cloneChild = this.cloneNode(srcChild, doc);
         if (cloneChild != null) {
             clone.appendChild(cloneChild);
         }
         srcChild = srcChild.nextSibling;
      }
      break;

      case 3:
      clone = doc.createTextNode(srcNode.nodeValue);
      break;

      case 4:
      clone = doc.createCDATASection(srcNode.nodeValue);
      break;

      case 5:
      clone = doc.createEntityReference(srcNode.nodeName);
      break;

      case 7:
      clone = doc.createProcessingInstruction(srcNode.target, srcNode.data);
      break;

      case 8:
      clone = doc.createComment(srcNode.nodeValue);
      break;
   }
   return clone;

}

HTMLBuilder.prototype.load = function(frame, varname, url) {
  if (this.documentVarnames[0] == varname) {
      return document;
  }
  //
  //
  //  not a perfect way to do this
  //    Document.cloneNode is implementation dependent but exists in L1
  //       and implemented in IE.  The alternative brute force copy
  //       only works in L2 or higher implementations and can't copy
  //       entity and notation definitions, etc.
  var clone = null;
  try {
      clone = document.cloneNode(true);
  } catch(ex) {
  }
  if (clone == null) {
      clone = document.implementation.createDocument(
          document.documentElement.namespaceURI,
          document.documentElement.nodeName,
          null);
      //
      //   Work-around since
      //   Safari does not create document element
      //      create document.
      if (clone.documentElement == null) {
           clone.appendChild(clone.createElementNS(
              document.documentElement.namespaceURI,
              document.documentElement.nodeName));
      }
      var attrs = document.documentElement.attributes;
      for(var i = 0; i < attrs.length; i++) {
          var srcAttr = attrs.item(i);
          clone.documentElement.setAttributeNS(
             srcAttr.namespaceURI, srcAttr.nodeName, srcAttr.nodeValue);
      }

      var srcNode = document.firstChild;
      while(srcNode != null && srcNode.nodeType != 1) {
          if (srcNode.nodeType != 10) {
               var cloneNode = this.cloneNode(srcNode, clone);
             clone.insertBefore(cloneNode, clone.documentElement);
           }
           srcNode = srcNode.nextSibling;
      }
      srcNode = document.documentElement.nextSibling;
      while(srcNode != null) {
          var cloneNode = this.cloneNode(srcNode, clone);
          clone.appendChild(cloneNode);
          srcNode = srcNode.nextSibling;
      }
      srcNode = document.documentElement.firstChild;
      while(srcNode != null) {
          var cloneNode = this.cloneNode(srcNode, clone);
          if (cloneNode != null) {
             clone.documentElement.appendChild(cloneNode);
          }
          srcNode = srcNode.nextSibling;
      }
  }
  return clone;
}

HTMLBuilder.prototype.getImplementationAttribute = function(attr) {
    for (var i = 0; i < this.fixedAttributeNames.length; i++) {
        if (this.fixedAttributeNames[i] == attr) {
            return this.fixedAttributeValues[i];
        }
    }
    throw "Unrecognized implementation attribute: " + attr;
}

HTMLBuilder.prototype.setImplementationAttribute = function(attribute, value) {
    var supported = this.getImplementationAttribute(attribute);
    if (supported != value) {
        this.initializationError = "HTML loader does not support " + attribute + "=" + value;
    }
}

HTMLBuilder.prototype.canSetImplementationAttribute = function(attribute, value) {
    var supported = this.getImplementationAttribute(attribute);
    return (supported == value);
}

function createConfiguredBuilder() {
    return new HTMLBuilder();
}

function catchInitializationError(buildr, ex) {
   buildr.initializationError = ex;
   buildr.initializationFatalError = ex;
}

function checkFeature(feature, version)
{
  if (!builder.hasFeature(feature, version))
  {
    //
    //   don't throw exception so that users can select to ignore the precondition
    //
    builder.initializationError = "builder does not support feature " + feature + " version " + version;
  }
}

function setResult(resultType, message) {
   var testName = getTargetURI();
   document.title = testName + ":" + resultType;
   var xhtmlNS = "http://www.w3.org/1999/xhtml";
   var newBody = document.createElementNS(xhtmlNS, "body");
   var newTable = document.createElementNS(xhtmlNS, "table");
   newTable.width = "100%";
   newTable.border = "1";
   newBody.appendChild(newTable);
   var testRow = newTable.insertRow(-1);
   var testDiv1 = testRow.insertCell(-1);
   testDiv1.appendChild(document.createTextNode("Test"));
   var testDiv2 = testRow.insertCell(-1);
   testDiv2.appendChild(document.createTextNode(testName));
   var statusRow = newTable.insertRow(-1);
   var statusDiv1 = statusRow.insertCell(-1);
   statusDiv1.appendChild(document.createTextNode("Status"));
   var statusDiv2 = statusRow.insertCell(-1);
   var style = "color:green";
   if (resultType == null) {
           statusDiv2.appendChild(document.createTextNode("Success"));
   } else {
           statusDiv2.appendChild(document.createTextNode(resultType));
           if (resultType == "skip") {
               style = "color:blue";
           } else {
               style = "color:red";
           }
   }
   newTable.setAttributeNS(null, "style", style);
   if (message != null) {
      var messageRow = newTable.insertRow(-1);
         var messageDiv1 = messageRow.insertCell(-1);
      messageDiv1.appendChild(document.createTextNode("Message"));
      var messageDiv2 = messageRow.insertCell(-1);
      messageDiv2.appendChild(document.createTextNode((message instanceof Error) ? ("Line " + message.line + ": " + message.name) : message));
   }
   //
   //  WebKit modification: 18-August-2005
   //
   //  Use document.body instead of the line of code below, since this is wrong for a frameset.
   //
   // var oldBody = document.getElementsByTagName("body")[0];
   var oldBody = document.body;

   oldBody.parentNode.replaceChild(newBody, oldBody);
   if (parent != window && typeof(parent.setResult) != 'undefined') {
       parent.setResult(testName, resultType, message);
   }
}

function checkInitialization(buildr, testname) {
   return buildr.initializationError;
}

function preload(docRef, varname, href) {
   return builder.preload(docRef, varname, href);
}

function load(docRef, varname, href) {
   return builder.load(docRef, varname, href);
}

function getImplementationAttribute(attr) {
    return builder.getImplementationAttribute(attr);
}

function setImplementationAttribute(attribute, value) {
    builder.setImplementationAttribute(attribute, value);
}

function createXPathEvaluator(doc) {
    try {
        return doc.getFeature("XPath", null);
    }
    catch(ex) {
    }
    return doc;
}

function getImplementation() {
    return builder.getImplementation();
}

function assertEquals(id, expected, actual) {
   var myActual;
   if (expected != actual) {
       myActual = actual;
       if (actual == null) {
          myActual = "null";
       }
       throw "failure:" + id + ": assertEquals failed, actual " + myActual + ", expected " + expected + ".";
   }
}

function assertNull(id, actual) {
   if (actual != null) {
       throw "failure:" + id + ": assertNull failed, actual " + actual;
   }
}

function assertTrue(id, actual) {
   if (!actual) {
       throw "failure:" + id + ": assertTrue failed";
   }
}

function assertFalse(id, actual) {
   if (actual) {
       throw "failure:" + id +  ": assertTrue failed";
   }
}

function assertNotNull(id, actual) {
   if (actual == null) {
       throw "failure:" + id + ": assertNotNull failed";
   }
}

function fail(id) {
    throw "failure:" + id +  ": fail";
}

function getSuffix(contentType) {
    switch(contentType) {
        case "text/html":
        return ".html";

        case "text/xml":
        return ".xml";

        case "image/svg+xml":
        return ".svg";

        case "text/mathml":
        return ".mml";
    }
    return ".xhtml";
}

function getResourceURI(name, scheme, contentType) {
    var base = document.documentURI;
    if (base == null) {
       base = "";
    } else {
       base = base.substring(0, base.lastIndexOf('/') + 1) + "files/";
    }
    return base + name + getSuffix(contentType);
}

function startTest() {

//
//  WebKit modification: 18-August-2005
//
//  Inform the test controller that this test has a text-format result and so should
//  be dumped as text, and also that the dump should not occur automatically.
//
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}
//
//  End WebKit modification
//

    //
    //   invoke test setup
    //
    setUpPage();
    try {
        runTest();
        if (builder.initializationError == null) {
           setResult(null, null);
        } else {
           setResult("skip", builder.initializationError);
        }
    } catch(ex) {
        if (typeof(ex.substring) != 'undefined' && ex.substring(0, 8) == "failure:") {
            setResult("failure", ex.substring(8));
        } else {
            setResult("error", ex);
        }
    }

//
//  WebKit modification: 18-August-2005
//
//  Inform the test controller that this test is complete, so it's time to dump.
//
    if (window.testRunner) {
        testRunner.notifyDone();
    }
//
//  End WebKit modification
//

}
