if (window.testRunner)
    testRunner.waitUntilDone();

var constructors = ["Image", "MessageChannel", "Option", "XMLHttpRequest", "Audio"];

window.onload = function () {
    testFrame = document.getElementById("testFrame");
    storedConstructors = [];
    // Identity checks
    for (var i = 0; i < constructors.length; i++) {
        var constructor = constructors[i];
        try {
            shouldBe("testFrame.contentWindow." + constructor, "testFrame.contentWindow."+constructor);
            shouldBeTrue("testFrame.contentWindow." + constructor + " !== window." + constructor);
            testFrame.contentWindow[constructor].testProperty = "property set successfully";
            shouldBe("testFrame.contentWindow." + constructor + ".testProperty", '"property set successfully"');
            storedConstructors[constructor] = testFrame.contentWindow[constructor];
        } catch (e) {
            testFailed("Testing " + constructor + " threw " + e);
        }
    }
    testFrame.onload = function() {
        if (window.GCController)
            GCController.collect();

        // Test properties after load
        for (var i = 0; i < constructors.length; i++) {
            var constructor = constructors[i];
            try {
                // Repeat the identity checks to be safe
                shouldBe("testFrame.contentWindow." + constructor, "testFrame.contentWindow."+constructor);
                shouldBeTrue("testFrame.contentWindow." + constructor + " !== window." + constructor);
                
                // Make sure that we haven't reused the constructors from the old document
                shouldBeTrue("testFrame.contentWindow." + constructor + " !== storedConstructors." + constructor);
                shouldBe("storedConstructors." + constructor + ".testProperty", '"property set successfully"');

                // Make sure we haven't kept anything over from the original document
                shouldBeUndefined("testFrame.contentWindow." + constructor + ".testProperty");
                // Make sure we're getting the same constructor as the frame does internally
                shouldBeTrue("testFrame.contentWindow." + constructor + ".cachedOnOwnerDocument");
            } catch (e) {
                testFailed("Testing " + constructor + " threw " + e);
            }
        }
        
        if (window.testRunner)
            testRunner.notifyDone();
    };
    testFrame.srcdoc =
        '<script>var constructors = ["Image", "MessageChannel", "Option", "XMLHttpRequest", "Audio"];' +
        'for(var i = 0; i < constructors.length; i++) if(window[constructors[i]])' +
        'window[constructors[i]].cachedOnOwnerDocument = true;</script>';
}
