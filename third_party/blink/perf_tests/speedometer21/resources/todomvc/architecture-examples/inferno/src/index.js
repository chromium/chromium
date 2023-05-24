import Inferno from 'inferno';
import Component from 'inferno-component';
import { ENTER, filters, read } from './share';
import { Head, Foot } from './base';
import Model from './model';
import Item from './item';

const { render } = Inferno;
const model = new Model();

class App extends Component {
    state = {
        route: read(),
        todos: model.get()
    };

    update = arr => this.setState({todos: arr});

    componentWillMount = () => {
        window.onhashchange = () => this.setState({route: read()});
    };

    add = e => {
        if (e.which !== ENTER) return;

        const val = e.target.value.trim();
        if (!val) return;

        e.target.value = '';
        this.update(
            model.add(val)
        );
    };

    edit = (todo, val) => {
        val = val.trim();
        if (val.length) {
            this.update(
                model.put(todo, {title: val, editing: 0})
            );
        } else {
            this.remove(todo);
        }
    };

    focus = todo => this.update(
        model.put(todo, {editing: 1})
    );

    blur = todo => this.update(
        model.put(todo, {editing: 0})
    );

    remove = todo => this.update(
        model.del(todo)
    );

    toggleOne = todo => this.update(
        model.toggle(todo)
    );

    toggleAll = ev => this.update(
        model.toggleAll(ev.target.checked)
    );

    clearCompleted = () => this.update(
        model.clearCompleted()
    );

    render(_, {todos, route}) {
        const num = todos.length;
        const shown = todos.filter(filters[route]);
        const numDone = todos.filter(filters.completed).length;
        const numAct = num - numDone;

        return (
            <div>
                <Head onEnter={ this.add } />

                { num ? (
                    <section className="main">
                        <input className="toggle-all" type="checkbox"
                            onClick={ this.toggleAll } checked={ numAct === 0 }
                        />

                        <ul className="todo-list">
                            {
                                shown.map(t =>
                                    <Item data={t}
                                        onBlur={ () => this.blur(t) }
                                        onFocus={ () => this.focus(t) }
                                        doDelete={ () => this.remove(t) }
                                        doSave={ val => this.edit(t, val) }
                                        doToggle={ () => this.toggleOne(t) }
                                    />
                                )
                            }
                        </ul>
                    </section>
                ) : null }

                { (numAct || numDone) ? (
                    <Foot onClear={ this.clearCompleted }
                        left={numAct} done={numDone} route={route}
                    />
                ) : null }
            </div>
        )
    }
}

render(<App />, document.getElementById('app'));
