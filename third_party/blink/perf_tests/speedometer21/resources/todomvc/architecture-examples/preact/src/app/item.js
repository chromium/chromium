import cx from 'classnames';
import { h, Component } from 'preact';

const ESCAPE_KEY = 27;
const ENTER_KEY = 13;

export default class TodoItem extends Component {
    handleSubmit = () => {
        let { onSave, onDestroy, todo } = this.props,
            val = this.state.editText.trim();
        if (val) {
            onSave(todo, val);
            this.setState({ editText: val });
        }
        else {
            onDestroy(todo);
        }
    };

    handleEdit = () => {
        let { onEdit, todo } = this.props;
        onEdit(todo);
        this.setState({ editText: todo.title });
    };

    toggle = e => {
        let { onToggle, todo } = this.props;
        onToggle(todo);
        e.preventDefault();
    };

    handleKeyDown = e => {
        if (e.which===ESCAPE_KEY) {
            let { todo } = this.props;
            this.setState({ editText: todo.title });
            this.props.onCancel(todo);
        }
        else if (e.which===ENTER_KEY) {
            this.handleSubmit();
        }
    };

    handleDestroy = () => {
        this.props.onDestroy(this.props.todo);
    };

    // shouldComponentUpdate({ todo, editing, editText }) {
    //  return (
    //      todo !== this.props.todo ||
    //      editing !== this.props.editing ||
    //      editText !== this.state.editText
    //  );
    // }

    componentDidUpdate() {
        let node = this.base && this.base.querySelector('.edit');
        if (node) node.focus();
    }

    render({ todo:{ title, completed }, onToggle, onDestroy, editing }, { editText }) {
        return (
            <li class={cx({ completed, editing })}>
                <div class="view">
                    <input
                        class="toggle"
                        type="checkbox"
                        checked={completed}
                        onChange={this.toggle}
                    />
                    <label onDblClick={this.handleEdit}>{title}</label>
                    <button class="destroy" onClick={this.handleDestroy} />
                </div>
                { editing && (
                    <input
                        class="edit"
                        value={editText}
                        onBlur={this.handleSubmit}
                        onInput={this.linkState('editText')}
                        onKeyDown={this.handleKeyDown}
                    />
                ) }
            </li>
        );
    }
}
