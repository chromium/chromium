/*
Copyright Â© 2001-2004 World Wide Web Consortium,
(Massachusetts Institute of Technology, European Research Consortium
for Informatics and Mathematics, Keio University). All
Rights Reserved. This work is distributed under the W3CÂ® Software License [1] in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

[1] http://www.w3.org/Consortium/Legal/2002/copyright-software-20021231
*/

   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level3/core/canonicalform06";
   }

var docsLoaded = -1000000;
var builder = null;

//
//   This function is called by the testing framework before
//      running the test suite.
//
//   If there are no configuration exceptions, asynchronous
//        document loading is started.  Otherwise, the status
//        is set to complete and the exception is immediately
//        raised when entering the body of the test.
//
function setUpPage() {
   setUpPageStatus = 'running';
   try {
     //
     //   creates test document builder, may throw exception
     //
     builder = createConfiguredBuilder();

      docsLoaded = 0;

       if (docsLoaded == 0) {
          setUpPageStatus = 'complete';
       }
    } catch(ex) {
        catchInitializationError(builder, ex);
        setUpPageStatus = 'complete';
    }
}

//
//   This method is called on the completion of
//      each asychronous load started in setUpTests.
//
//   When every synchronous loaded document has completed,
//      the page status is changed which allows the
//      body of the test to be executed.
function loadComplete() {
    if (++docsLoaded == 0) {
        setUpPageStatus = 'complete';
    }
}

//DOMErrorMonitor's require a document level variable named errorMonitor
var errorMonitor;

/**
*
Create a document with an XML 1.1 valid but XML 1.0 invalid element and
normalize document with canonical-form set to true.

* @author Curt Arnold
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#Document3-normalizeDocument
* @see http://www.w3.org/TR/2004/REC-DOM-Level-3-Core-20040407/core#parameter-canonical-form
*/
function canonicalform06() {
   var success;
    if(checkInitialization(builder, "canonicalform06") != null) return;
    var domImpl;
      var nullString = null;

      var nullDoctype = null;

      var doc;
      var elem;
      var retval;
      var domConfig;
      errorMonitor = new DOMErrorMonitor();

      var errors = new Array();

      var error;
      var severity;
      var type;
      var locator;
      var relatedNode;
      var canSet;
      domImpl = getImplementation();
doc = domImpl.createDocument(nullString,nullString,nullDoctype);

    {
        success = false;
        try {
            elem = doc.createElementNS("http://www.example.org/domts/wellformed01","LegalName}");
        }
        catch(ex) {
      success = (typeof(ex.code) != 'undefined' && ex.code == 5);
        }
        assertTrue("xml10InvalidName",success);
    }

      try {
      doc.xmlVersion = "1.1";

      } catch (ex) {
          if (typeof(ex.code) != 'undefined') {
       switch(ex.code) {
       case /* NOT_SUPPORTED_ERR */ 9 :
               return ;
    default:
          throw ex;
          }
       } else {
       throw ex;
        }
         }
        elem = doc.createElementNS("http://www.example.org/domts/wellformed01","LegalName}");
      retval = doc.appendChild(elem);
      doc.xmlVersion = "1.0";

      domConfig = doc.domConfig;

      canSet = domConfig.canSetParameter("canonical-form",true);

    if(
    canSet
    ) {
    domConfig.setParameter("canonical-form", true);
     domConfig.setParameter("error-handler", errorMonitor.handleError);
     doc.normalizeDocument();
      errors = errorMonitor.allErrors;
for(var indexN100B7 = 0;indexN100B7 < errors.length; indexN100B7++) {
      error = errors[indexN100B7];
      severity = error.severity;

      assertEquals("severity",2,severity);
       type = error.type;

      assertEquals("type","wf-invalid-character-in-node-name",type);
       locator = error.location;

      relatedNode = locator.relatedNode;

      assertSame("relatedNode",elem,relatedNode);

    }
   assertSize("oneError",1,errors);

    }

}

function runTest() {
   canonicalform06();
}
