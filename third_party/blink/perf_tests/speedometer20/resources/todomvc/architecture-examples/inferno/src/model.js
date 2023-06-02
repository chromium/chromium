import { assign, isEqual } from './share';

const STOR = {};
const STOR_ID = 'todos-inferno';

export default class Model {
    get = () => (this.data = JSON.parse(STOR[STOR_ID] || '[]'))

    set = arr => {
        this.data = arr || this.data || [];
        STOR[STOR_ID] = JSON.stringify(this.data);
        return this.data;
    }

    add = str => this.set(
        this.data.concat({title: str, completed: false})
    )

    put = (todo, obj) => this.set(
        this.data.map(t => isEqual(t, todo) ? assign(todo, obj) : t)
    )

    del = todo => this.set(
        this.data.filter(t => !isEqual(t, todo))
    )

    toggle = todo => this.put(todo, {completed: !todo.completed})

    toggleAll = completed => this.set(
        this.data.map(t => ({...t, completed}))
    )

    clearCompleted = () => this.set(
        this.data.filter(t => !t.completed)
    )
}
