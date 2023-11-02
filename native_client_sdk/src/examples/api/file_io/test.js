// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function addTests() {
  var currentTest = null;

  function dispatchClick(element) {
    element.dispatchEvent(new MouseEvent('click'));
  }

  function getVisibleElementByTagName(tag) {
    var selector = '.function:not([hidden]) ' + tag;
    return document.querySelector(selector);
  }

  function clickRadio(name) {
    currentTest.log('Clicking ' + name + ' radio button.');
    dispatchClick(document.getElementById('radio_' + name));
  }

  function setInputValue(value, selector) {
    if (!selector) {
      selector = 'input';
    }

    currentTest.log('Setting input box to "' + value + '".');
    getVisibleElementByTagName(selector).value = value;
  }

  function setTextareaValue(value) {
    currentTest.log('Setting textarea to "' + value + '".');
    getVisibleElementByTagName('textarea').value = value;
  }

  function getTextareaValue() {
    return getVisibleElementByTagName('textarea').value;
  }

  function getLastLogMessage() {
    var logEl = document.getElementById('log');
    var logLines = logEl.textContent.split('\n');
    return logLines[logLines.length - 1];
  }

  function waitForLog(logMessage, onLogChanged, onError) {
    // Clear the log. This prevents a previous failure from propagating to the
    // current check. (NOTE: the log is backed by an array, so as soon as a
    // message is logged it will be refilled with its previous data in addition
    // to the new message.)
    document.getElementById('log').textContent = '';

    // Poll for log changes.
    var intervalId = window.setInterval(function() {
      var lastLogMessage = getLastLogMessage();

      if (lastLogMessage.lastIndexOf('Error:', 0) === 0) {
        window.clearInterval(intervalId);
        if (onError) {
          currentTest.log('Got error message, continuing.');
          onError();
        } else {
          currentTest.fail('Unexpected failure waiting for log change.');
        }

        return;
      }

      if (logMessage !== lastLogMessage)
        return;

      currentTest.log('Got log message, continuing.');
      window.clearInterval(intervalId);
      onLogChanged();
    }, 100);
  }

  function clickExecuteButtonAndWaitForLog(logMessage, onLogChanged, onError) {
    waitForLog(logMessage, onLogChanged, onError);
    currentTest.log('Clicking button.');
    dispatchClick(getVisibleElementByTagName('button'));
    currentTest.log('Waiting for log message: "' + logMessage + '".');
  }

  function isFilenameInDirectoryList(filename) {
    var listItemEls = document.querySelectorAll('#listDirOutput li');

    currentTest.log('Looking for ' + filename);
    for (var i = 0; i < listItemEls.length; ++i) {
      var itemText = listItemEls[i].textContent;
      currentTest.log('Found ' + itemText);
      if (itemText === filename) {
        return true;
      }
    }

    return false;
  }

  function saveFile(filename, fileText, onFileSaved, onError) {
    clickRadio('saveFile');
    setInputValue(filename);
    setTextareaValue(fileText);
    clickExecuteButtonAndWaitForLog('Save success', onFileSaved, onError);
  }

  function loadFile(filename, onFileLoaded, onError) {
    clickRadio('loadFile');
    setInputValue(filename);
    setTextareaValue('');  // Clear the textarea.
    clickExecuteButtonAndWaitForLog('Load success', onFileLoaded, onError);
  }

  function deleteFile(filename, onFileDeleted, onError) {
    clickRadio('delete');
    setInputValue(filename);
    clickExecuteButtonAndWaitForLog('Delete success', onFileDeleted, onError);
  }

  function listDir(dirname, onDirectoryListed, onError) {
    clickRadio('listDir');
    setInputValue(dirname);
    clickExecuteButtonAndWaitForLog('List success', onDirectoryListed, onError);
  }

  function makeDir(dirname, onDirectoryMade, onError) {
    clickRadio('makeDir');
    setInputValue(dirname);
    clickExecuteButtonAndWaitForLog('Make directory success',
                                    onDirectoryMade, onError);
  }

  function rename(oldname, newname, onRenamed, onError) {
    clickRadio('rename');
    setInputValue(oldname, '#renameOld');
    setInputValue(newname, '#renameNew');
    clickExecuteButtonAndWaitForLog('Rename success', onRenamed, onError);
  }

  function expectEq(expected, actual, additionalInfo) {
    var message;
    if (expected !== actual) {
      if (additionalInfo)
        message = additionalInfo + ': ';
      message += 'Expected "' + expected + '", got "' + actual + '".';
      currentTest.fail(message);
    } else {
      message = 'OK, "' + expected + '" === "' + actual + '".';
      currentTest.log(message);
    }
  }

  function expectContains(needle, haystack, additionalInfo) {
    if (haystack.indexOf(needle) === -1) {
      if (additionalInfo)
        message = additionalInfo + ': ';
      message += 'Expected to find "' + needle + '" in "' + haystack + '".';
      currentTest.fail(message);
    } else {
      message = 'OK, "' + needle + '" in "' + haystack + '".';
      currentTest.log(message);
    }
  }

  function expectFilenameInDirectoryList(filename, additionalInfo) {
    if (!isFilenameInDirectoryList(filename)) {
      if (additionalInfo)
        message = additionalInfo + ': ';
      message += 'Expected to find "' + filename + '" in directory list.';
      currentTest.fail(message);
    } else {
      message = 'OK, found "' + filename + ' in the directory list.';
      currentTest.log(message);
    }
  }

  common.tester.addAsyncTest('filesystem_ready', function(test) {
    // This is a bit fragile; we rely on this test being run first (and
    // completing) before we can run any of the other tests.
    currentTest = test;
    var message = 'Filesystem ready!';

    // This message may already be logged.
    if (getLastLogMessage() == message) {
      test.pass();
      return;
    }

    waitForLog('Filesystem ready!', function() {
      test.pass();
    }, function() {
      test.fail('Got unexpected error waiting for filesystem: ');
    });
  });

  common.tester.addAsyncTest('save_and_load', function(test) {
    currentTest = test;
    var filename = '/save_and_load.txt';
    var fileText = 'A penny saved is a penny earned.';

    // Save the file.
    saveFile(filename, fileText, function() {
      // Now try to load it.
      loadFile(filename, function() {
        // Make sure the text matches.
        expectEq(fileText, getTextareaValue(), 'Incorrect textarea');
        test.pass();
      });
    });
  });

  common.tester.addAsyncTest('delete_file', function(test) {
    currentTest = test;
    var filename = '/delete_file.txt';

    saveFile(filename, 'Here today, gone tomorrow.', function() {
      deleteFile(filename, function() {
        loadFile(filename, function() {
          test.fail('Unexpected load success.');
        },
        function() {
          expectEq('', getTextareaValue(), 'Unexpected data in file');
          expectContains('File not found', getLastLogMessage(),
                         'Unexpected log message');
          test.pass();
        });
      });
    });
  });

  common.tester.addAsyncTest('list_directory', function(test) {
    currentTest = test;
    var filename = '/list_directory.txt';

    saveFile(filename, 'I\'ve got a little list...', function() {
      listDir('/', function() {
        // Directory listings are relative, so it will not have the leading
        // slash.
        var relativeFilename = filename.slice(1);
        expectFilenameInDirectoryList(relativeFilename);
        test.pass();
      });
    });
  });

  common.tester.addAsyncTest('make_directory', function(test) {
    currentTest = test;
    var dirname = '/new_directory';

    makeDir(dirname, function() {
      listDir('/', function() {
        // Directory listings are relative, so it will not have the leading
        // slash.
        var relativeDirname = dirname.slice(1);
        expectFilenameInDirectoryList(relativeDirname);

        // Let's see if the file can be written to this directory.
        var filename = dirname + '/file.txt';
        var fileText = 'A file within a directory.';
        saveFile(filename, fileText, function() {
          test.pass();
        });
      });
    });
  });

  common.tester.addAsyncTest('rename_file', function(test) {
    currentTest = test;
    var filename = '/rename_old.txt';
    var newFilename = '/rename_new.txt';
    var fileText = 'What\'s in a name? that which we call a rose ' +
                   'by any other name would smell as sweet;';

    // Save the file.
    saveFile(filename, fileText, function() {
      // Now rename it.
      rename(filename, newFilename, function() {
        // Now try to load it.
        loadFile(newFilename, function() {
          // Make sure the text matches.
          expectEq(fileText, getTextareaValue(), 'Incorrect textarea');

          // Make sure the old file no longer exists.
          loadFile(filename, function() {
            test.fail('Unexpected load success.');
          },
          function() {
            expectEq('', getTextareaValue(), 'Unexpected data in file');
            expectContains('File not found', getLastLogMessage(),
                           'Unexpected log message');
            test.pass();
          });
        });
      });
    });
  });
}
