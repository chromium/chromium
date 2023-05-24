var numberOfItemsToAdd = 100;
var Suites = [];
var ENTER_KEY_CODE = 13;

var triggerEnter = function (element, type) {
    var event = document.createEvent('Events');
    event.initEvent(type, true, true);
    event.keyCode = ENTER_KEY_CODE;
    event.which = ENTER_KEY_CODE;
    event.key = 'ENTER';
    element.dispatchEvent(event);
}

Suites.push({
    name: 'VanillaJS-TodoMVC',
    url: 'todomvc/vanilla-examples/vanillajs/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('change'));
                triggerEnter(newTodo, 'keypress');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'Vanilla-ES2015-TodoMVC',
    url: 'todomvc/vanilla-examples/es2015/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('change'));
                triggerEnter(newTodo, 'keypress');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'Vanilla-ES2015-Babel-Webpack-TodoMVC',
    url: 'todomvc/vanilla-examples/es2015-babel-webpack/dist/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('change'));
                triggerEnter(newTodo, 'keypress');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'React-TodoMVC',
    url: 'todomvc/architecture-examples/react/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        contentWindow.app.Utils.store = function () {}
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('input', {
                  bubbles: true,
                  cancelable: true
                }));
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'React-Redux-TodoMVC',
    url: 'todomvc/architecture-examples/react-redux/dist/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'EmberJS-TodoMVC',
    url: 'todomvc/architecture-examples/emberjs/dist/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'EmberJS-Debug-TodoMVC',
    url: 'todomvc/architecture-examples/emberjs-debug/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'BackboneJS-TodoMVC',
    url: 'todomvc/architecture-examples/backbone/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
    contentWindow.Backbone.sync = function () {}
        return runner.waitForElement('#appIsReady').then(function (element) {
            var newTodo = contentDocument.querySelector('.new-todo');
            newTodo.focus();
            return newTodo;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keypress');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < numberOfItemsToAdd; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < numberOfItemsToAdd; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'AngularJS-TodoMVC',
    url: 'todomvc/architecture-examples/angularjs/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            var submitEvent = document.createEvent('Event');
            submitEvent.initEvent('submit', true, true);
            var inputEvent = document.createEvent('Event');
            inputEvent.initEvent('input', true, true);
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(inputEvent);
                newTodo.form.dispatchEvent(submitEvent);
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++)
                contentDocument.querySelector('.destroy').click();
        }),
    ]
});

Suites.push({
    name: 'Angular2-TypeScript-TodoMVC',
    url: 'todomvc/architecture-examples/angular/dist/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('input', {
                  bubbles: true,
                  cancelable: true
                }));
                triggerEnter(newTodo, 'keyup');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'VueJS-TodoMVC',
    url: 'todomvc/architecture-examples/vuejs-cli/dist/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('input', {
                  bubbles: true,
                  cancelable: true
                }));
                triggerEnter(newTodo, 'keyup');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'jQuery-TodoMVC',
    url: 'todomvc/architecture-examples/jquery/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#appIsReady').then(function (element) {
            var newTodo = contentDocument.getElementById('new-todo');
            newTodo.focus();
            return newTodo;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keyup');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (newTodo, contentWindow, contentDocument) {
            var checkboxes = contentDocument.getElementsByClassName('toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingAllItems', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++)
                contentDocument.querySelector('.destroy').click();
        }),
    ]
})

Suites.push({
    name: 'Preact-TodoMVC',
    url: 'todomvc/architecture-examples/preact/dist/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

Suites.push({
    name: 'Inferno-TodoMVC',
    url: 'todomvc/architecture-examples/inferno/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('change', {
                  bubbles: true,
                  cancelable: true
                }));
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < numberOfItemsToAdd; i++)
                contentDocument.querySelector('.destroy').click();
        }),
    ]
});

function processElmWorkQueue(contentWindow)
{
    contentWindow.elmWork();
    var callbacks = contentWindow.rAFCallbackList;
    var i = 0;
    while (i < callbacks.length) {
        callbacks[i]();
        i++;
    }
    contentWindow.rAFCallbackList = [];
}

Suites.push({
    name: 'Elm-TodoMVC',
    url: 'todomvc/functional-prog-examples/elm/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.new-todo').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                newTodo.dispatchEvent(new Event('input', {
                  bubbles: true,
                  cancelable: true
                }));
                processElmWorkQueue(contentWindow);
                triggerEnter(newTodo, 'keydown');
                processElmWorkQueue(contentWindow);
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++) {
                checkboxes[i].click();
                processElmWorkQueue(contentWindow);
            }
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                contentDocument.querySelector('.destroy').click();
                processElmWorkQueue(contentWindow);
            }
        }),
    ]
});

Suites.push({
    name: 'Flight-TodoMVC',
    url: 'todomvc/dependency-examples/flight/flight/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('#appIsReady').then(function (element) {
            var newTodo = contentDocument.getElementById('new-todo');
            newTodo.focus();
            return newTodo;
        });
    },
    tests: [
        new BenchmarkTestStep('Adding' + numberOfItemsToAdd + 'Items', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < numberOfItemsToAdd; i++) {
                newTodo.value = 'Something to do ' + i;
                triggerEnter(newTodo, 'keydown');
            }
        }),
        new BenchmarkTestStep('CompletingAllItems', function (params, contentWindow, contentDocument) {
            var checkboxes = contentDocument.querySelectorAll('.toggle');
            for (var i = 0; i < checkboxes.length; i++)
                checkboxes[i].click();
        }),
        new BenchmarkTestStep('DeletingItems', function (params, contentWindow, contentDocument) {
            var deleteButtons = contentDocument.querySelectorAll('.destroy');
            for (var i = 0; i < deleteButtons.length; i++)
                deleteButtons[i].click();
        }),
    ]
});

var actionCount = 50;
Suites.push({
    disabled: true,
    name: 'FlightJS-MailClient',
    url: 'flightjs-example-app/index.html',
    prepare: function (runner, contentWindow, contentDocument) {
        return runner.waitForElement('.span8').then(function (element) {
            element.focus();
            return element;
        });
    },
    tests: [
        new BenchmarkTestStep('OpeningTabs' + actionCount + 'Times', function (newTodo, contentWindow, contentDocument) {
            contentDocument.getElementById('inbox').click();
            for (var i = 0; i < actionCount; i++) {
                contentDocument.getElementById('later').click();
                contentDocument.getElementById('sent').click();
                contentDocument.getElementById('trash').click();
                contentDocument.getElementById('inbox').click();
            }
        }),
        new BenchmarkTestStep('MovingEmails' + actionCount + 'Times', function (newTodo, contentWindow, contentDocument) {
            contentDocument.getElementById('inbox').click();
            for (var i = 0; i < actionCount; i++) {
                contentDocument.getElementById('mail_2139').click();
                contentDocument.getElementById('move_mail').click();
                contentDocument.querySelector('#move_to_selector #later').click();
                contentDocument.getElementById('later').click();
                contentDocument.getElementById('mail_2139').click();
                contentDocument.getElementById('move_mail').click();
                contentDocument.querySelector('#move_to_selector #trash').click();
                contentDocument.getElementById('trash').click();
                contentDocument.getElementById('mail_2139').click();
                contentDocument.getElementById('move_mail').click();
                contentDocument.querySelector('#move_to_selector #inbox').click();
                contentDocument.getElementById('inbox').click();
            }
        }),
        new BenchmarkTestStep('Sending' + actionCount + 'NewEmails', function (newTodo, contentWindow, contentDocument) {
            for (var i = 0; i < actionCount; i++) {
                contentDocument.getElementById('new_mail').click();
                contentDocument.getElementById('recipient_select').selectedIndex = 1;
                var subject = contentDocument.getElementById('compose_subject');
                var message = contentDocument.getElementById('compose_message');
                subject.focus();
                contentWindow.$(subject).trigger('keydown');
                contentWindow.$(subject).text('Hello');
                message.focus();
                contentWindow.$(message).trigger('keydown');
                contentWindow.$(message).text('Hello,\n\nThis is a test message.\n\n- WebKitten');
                contentDocument.getElementById('send_composed').click();
            }
        }),
    ]
});
