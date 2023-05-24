/*jshint quotmark:false */
/*jshint white:false */
/*jshint trailing:false */
/*jshint newcap:false */
/*global React, Router*/
var app = app || {};

(function () {
	'use strict';

	app.ALL_TODOS = 'all';
	app.ACTIVE_TODOS = 'active';
	app.COMPLETED_TODOS = 'completed';

  app.Utils = {
    uuid: function () {
      /*jshint bitwise:false */
      var i, random;
      var uuid = '';

      for (i = 0; i < 32; i++) {
        random = Math.random() * 16 | 0;
        if (i === 8 || i === 12 || i === 16 || i === 20) {
          uuid += '-';
        }
        uuid += (i === 12 ? 4 : (i === 16 ? (random & 3 | 8) : random))
          .toString(16);
      }

      return uuid;
    },

    pluralize: function (count, word) {
      return count === 1 ? word : word + 's';
    },

    extend: function () {
      var newObj = {};
      for (var i = 0; i < arguments.length; i++) {
        var obj = arguments[i];
        for (var key in obj) {
          if (obj.hasOwnProperty(key)) {
            newObj[key] = obj[key];
          }
        }
      }
      return newObj;
    }
  };

  var Utils = app.Utils;
  // Generic "model" object. You can use whatever
  // framework you want. For this application it
  // may not even be worth separating this logic
  // out, but we do this to demonstrate one way to
  // separate out parts of your application.
  app.TodoModel = function (key) {
    this.key = key;
    this.todos = [];
    this.onChanges = [];
  };

  app.TodoModel.prototype.subscribe = function (onChange) {
    this.onChanges.push(onChange);
  };

  app.TodoModel.prototype.inform = function () {
    this.onChanges.forEach(function (cb) { cb(); });
  };

  app.TodoModel.prototype.addTodo = function (title) {
    this.todos = this.todos.concat({
      id: Utils.uuid(),
      title: title,
      completed: false
    });

    this.inform();
  };

  app.TodoModel.prototype.toggleAll = function (checked) {
    // Note: it's usually better to use immutable data structures since they're
    // easier to reason about and React works very well with them. That's why
    // we use map() and filter() everywhere instead of mutating the array or
    // todo items themselves.
    this.todos = this.todos.map(function (todo) {
      return Utils.extend({}, todo, {completed: checked});
    });

    this.inform();
  };

  app.TodoModel.prototype.toggle = function (todoToToggle) {
    this.todos = this.todos.map(function (todo) {
      return todo !== todoToToggle ?
        todo :
        Utils.extend({}, todo, {completed: !todo.completed});
    });

    this.inform();
  };

  app.TodoModel.prototype.destroy = function (todo) {
    this.todos = this.todos.filter(function (candidate) {
      return candidate !== todo;
    });

    this.inform();
  };

  app.TodoModel.prototype.save = function (todoToSave, text) {
    this.todos = this.todos.map(function (todo) {
      return todo !== todoToSave ? todo : Utils.extend({}, todo, {title: text});
    });

    this.inform();
  };

  app.TodoModel.prototype.clearCompleted = function () {
    this.todos = this.todos.filter(function (todo) {
      return !todo.completed;
    });

    this.inform();
  };


  var TodoFooter = React.createClass({
    render: function () {
      var activeTodoWord = app.Utils.pluralize(this.props.count, 'item');
      var clearButton = null;

      if (this.props.completedCount > 0) {
        clearButton = (
          <button
            className="clear-completed"
            onClick={this.props.onClearCompleted}>
            Clear completed
          </button>
        );
      }

      var nowShowing = this.props.nowShowing;
      return (
        <footer className="footer">
          <span className="todo-count">
            <strong>{this.props.count}</strong> {activeTodoWord} left
          </span>
          <ul className="filters">
            <li>
              <a
                href="#/"
                className={classNames({selected: nowShowing === app.ALL_TODOS})}>
                  All
              </a>
            </li>
            {' '}
            <li>
              <a
                href="#/active"
                className={classNames({selected: nowShowing === app.ACTIVE_TODOS})}>
                  Active
              </a>
            </li>
            {' '}
            <li>
              <a
                href="#/completed"
                className={classNames({selected: nowShowing === app.COMPLETED_TODOS})}>
                  Completed
              </a>
            </li>
          </ul>
          {clearButton}
        </footer>
      );
    }
  });

  var ESCAPE_KEY = 27;
  var ENTER_KEY = 13;

  var TodoItem = React.createClass({
    handleSubmit: function (event) {
      var val = this.state.editText.trim();
      if (val) {
        this.props.onSave(val);
        this.setState({editText: val});
      } else {
        this.props.onDestroy();
      }
    },

    handleEdit: function () {
      this.props.onEdit();
      this.setState({editText: this.props.todo.title});
    },

    handleKeyDown: function (event) {
      if (event.which === ESCAPE_KEY) {
        this.setState({editText: this.props.todo.title});
        this.props.onCancel(event);
      } else if (event.which === ENTER_KEY) {
        this.handleSubmit(event);
      }
    },

    handleChange: function (event) {
      if (this.props.editing) {
        this.setState({editText: event.target.value});
      }
    },

    getInitialState: function () {
      return {editText: this.props.todo.title};
    },

    /**
     * This is a completely optional performance enhancement that you can
     * implement on any React component. If you were to delete this method
     * the app would still work correctly (and still be very performant!), we
     * just use it as an example of how little code it takes to get an order
     * of magnitude performance improvement.
     */
    shouldComponentUpdate: function (nextProps, nextState) {
      return (
        nextProps.todo !== this.props.todo ||
        nextProps.editing !== this.props.editing ||
        nextState.editText !== this.state.editText
      );
    },

    /**
     * Safely manipulate the DOM after updating the state when invoking
     * `this.props.onEdit()` in the `handleEdit` method above.
     * For more info refer to notes at https://facebook.github.io/react/docs/component-api.html#setstate
     * and https://facebook.github.io/react/docs/component-specs.html#updating-componentdidupdate
     */
    componentDidUpdate: function (prevProps) {
      if (!prevProps.editing && this.props.editing) {
        var node = React.findDOMNode(this.refs.editField);
        node.focus();
        node.setSelectionRange(node.value.length, node.value.length);
      }
    },

    render: function () {
      return (
        <li className={classNames({
          completed: this.props.todo.completed,
          editing: this.props.editing
        })}>
          <div className="view">
            <input
              className="toggle"
              type="checkbox"
              checked={this.props.todo.completed}
              onChange={this.props.onToggle}
            />
            <label onDoubleClick={this.handleEdit}>
              {this.props.todo.title}
            </label>
            <button className="destroy" onClick={this.props.onDestroy} />
          </div>
          <input
            ref="editField"
            className="edit"
            value={this.state.editText}
            onBlur={this.handleSubmit}
            onChange={this.handleChange}
            onKeyDown={this.handleKeyDown}
          />
        </li>
      );
    }
  });

	var ENTER_KEY = 13;

	var TodoApp = React.createClass({
		getInitialState: function () {
			return {
				nowShowing: app.ALL_TODOS,
				editing: null,
				newTodo: ''
			};
		},

		componentDidMount: function () {
			var setState = this.setState;
			var router = Router({
				'/': setState.bind(this, {nowShowing: app.ALL_TODOS}),
				'/active': setState.bind(this, {nowShowing: app.ACTIVE_TODOS}),
				'/completed': setState.bind(this, {nowShowing: app.COMPLETED_TODOS})
			});
			router.init('/');
		},

		handleChange: function (event) {
			this.setState({newTodo: event.target.value});
		},

		handleNewTodoKeyDown: function (event) {
			if (event.keyCode !== ENTER_KEY) {
				return;
			}

			event.preventDefault();

			var val = this.state.newTodo.trim();

			if (val) {
				this.props.model.addTodo(val);
				this.setState({newTodo: ''});
			}
		},

		toggleAll: function (event) {
			var checked = event.target.checked;
			this.props.model.toggleAll(checked);
		},

		toggle: function (todoToToggle) {
			this.props.model.toggle(todoToToggle);
		},

		destroy: function (todo) {
			this.props.model.destroy(todo);
		},

		edit: function (todo) {
			this.setState({editing: todo.id});
		},

		save: function (todoToSave, text) {
			this.props.model.save(todoToSave, text);
			this.setState({editing: null});
		},

		cancel: function () {
			this.setState({editing: null});
		},

		clearCompleted: function () {
			this.props.model.clearCompleted();
		},

		render: function () {
			var footer;
			var main;
			var todos = this.props.model.todos;

			var shownTodos = todos.filter(function (todo) {
				switch (this.state.nowShowing) {
				case app.ACTIVE_TODOS:
					return !todo.completed;
				case app.COMPLETED_TODOS:
					return todo.completed;
				default:
					return true;
				}
			}, this);

			var todoItems = shownTodos.map(function (todo) {
				return (
					<TodoItem
						key={todo.id}
						todo={todo}
						onToggle={this.toggle.bind(this, todo)}
						onDestroy={this.destroy.bind(this, todo)}
						onEdit={this.edit.bind(this, todo)}
						editing={this.state.editing === todo.id}
						onSave={this.save.bind(this, todo)}
						onCancel={this.cancel}
					/>
				);
			}, this);

			var activeTodoCount = todos.reduce(function (accum, todo) {
				return todo.completed ? accum : accum + 1;
			}, 0);

			var completedCount = todos.length - activeTodoCount;

			if (activeTodoCount || completedCount) {
				footer =
					<TodoFooter
						count={activeTodoCount}
						completedCount={completedCount}
						nowShowing={this.state.nowShowing}
						onClearCompleted={this.clearCompleted}
					/>;
			}

			if (todos.length) {
				main = (
					<section className="main">
						<input
							className="toggle-all"
							type="checkbox"
							onChange={this.toggleAll}
							checked={activeTodoCount === 0}
						/>
						<ul className="todo-list">
							{todoItems}
						</ul>
					</section>
				);
			}

			return (
				<div>
					<header className="header">
						<h1>todos</h1>
						<input
							className="new-todo"
							placeholder="What needs to be done?"
							value={this.state.newTodo}
							onKeyDown={this.handleNewTodoKeyDown}
							onChange={this.handleChange}
							autoFocus={true}
						/>
					</header>
					{main}
					{footer}
				</div>
			);
		}
	});

	var model = new app.TodoModel('react-todos');

	function render() {
		ReactDOM.render(
			<TodoApp model={model}/>,
			document.getElementsByClassName('todoapp')[0]
		);
	}

	model.subscribe(render);
	render();
})();
