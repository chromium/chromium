var shorthandWriteExpectations = [
    { writeTextDecoration: '',
      readLine: 'none', readStyle: 'solid', readColor: 'rgb(0, 0, 0)' },
    { writeTextDecoration: 'underline',
      readLine: 'underline', readStyle: 'solid', readColor: 'rgb(0, 0, 0)' },
    { writeTextDecoration: 'underline overline',
      readLine: 'underline overline', readStyle: 'solid',
      readColor: 'rgb(0, 0, 0)' },
    { writeTextDecoration: 'underline double',
      readLine: 'underline', readStyle: 'double', readColor: 'rgb(0, 0, 0)' },
    { writeTextDecoration: 'underline double red',
      readLine: 'underline', readStyle: 'double',
      readColor: 'rgb(255, 0, 0)' },
    { writeTextDecoration: 'red underline double',
      readLine: 'underline', readStyle: 'double',
      readColor: 'rgb(255, 0, 0)' },
    { writeTextDecoration: 'double',
      readLine: 'none', readStyle: 'double', readColor: 'rgb(0, 0, 0)' },
    { writeTextDecoration: 'red',
      readLine: 'none', readStyle: 'solid', readColor: 'rgb(255, 0, 0)' },
    { writeTextDecoration: 'double red',
      readLine: 'none', readStyle: 'double', readColor: 'rgb(255, 0, 0)' },
];

var shorthandReadExpectations = [
    { writeLine: '', writeStyle: '', writeColor: '',
      readTextDecoration: 'none solid rgb(0, 0, 0)' },
    { writeLine: 'underline', writeStyle: '', writeColor: '',
      readTextDecoration: 'underline solid rgb(0, 0, 0)' },
    { writeLine: 'underline overline line-through', writeStyle: '',
      writeColor: '',
      readTextDecoration: 'underline overline line-through solid rgb(0, 0, 0)' },
    { writeLine: '', writeStyle: 'dashed', writeColor: '',
      readTextDecoration: 'none dashed rgb(0, 0, 0)' },
    { writeLine: '', writeStyle: '', writeColor: 'red',
      readTextDecoration: 'none solid rgb(255, 0, 0)' },
    { writeLine: '', writeStyle: '', writeColor: '#666666',
      readTextDecoration: 'none solid rgb(102, 102, 102)' },
    { writeLine: '', writeStyle: '', writeColor: 'rgba(1,1,1,0.4)',
      readTextDecoration: 'none solid rgba(1, 1, 1, 0.4)' },
    { writeLine: 'overline', writeStyle: 'double', writeColor: 'green',
      readTextDecoration: 'overline double rgb(0, 128, 0)' },
];

var writeInvalidExpectations = {
    'text-decoration': [ 'underlineTYPO',
                          'underlineTYPO red',
                          'more than four values listed'],
    'text-decoration-line': [ 'solid', 'typo', 'two values' ],
    'text-decoration-style': [ 'blue', 'typo', 'two values' ],
    'text-decoration-color': [ 'solid', 'typo', 'two values' ]
};

setup({ explicit_done: true });

function writeTextDecorationTests() {
    for (testParameters of shorthandWriteExpectations) {
        testElement.style.textDecoration = testParameters.writeTextDecoration;
        test(function() {
            assert_equals(getComputedStyle(testElement).textDecorationLine,
                          testParameters.readLine,
                          "computedStyle's textDecorationLine mismatch:");
            assert_equals(getComputedStyle(testElement).textDecorationStyle,
                          testParameters.readStyle,
                          "computedStyle's textDecorationStyle mismatch:");
            assert_equals(getComputedStyle(testElement).textDecorationColor,
                          testParameters.readColor,
                          "computedStyle's textDecorationColor mismatch:");
        }, "Sub properties set from longhand value: " +
             testParameters.writeTextDecoration);
    }
}

function readTextDecorationTests() {
    for (testParameters of shorthandReadExpectations) {
        testElement.style.textDecorationLine = testParameters.writeLine;
        testElement.style.textDecorationStyle = testParameters.writeStyle;
        testElement.style.textDecorationColor = testParameters.writeColor;
        test(function(){
            assert_equals(getComputedStyle(testElement).textDecoration,
                          testParameters.readTextDecoration,
                          "computedStyle's textDecoration mismatch:");
        }, "Shorthand from written subproperties: " +
             getComputedStyle(testElement).textDecoration);
    }
}

function invalidWriteTests() {
    for (key in writeInvalidExpectations) {
        for (value of writeInvalidExpectations[key]) {
            test(function(){
                assert_false(CSS.supports(key, value));
            }, "Invalid writes: Value " + value +
                 " invalid for property " + key + ".");
        }
    }
}

window.addEventListener("load", function() {
    writeTextDecorationTests();
    readTextDecorationTests();
    invalidWriteTests();
    done();
});
