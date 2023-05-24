import Inferno from 'inferno';
import { states } from './share';

/**
 * Stateless Header component
 */
export function Head({onEnter}) {
    return (
        <header className="header">
            <h1>todos</h1>
            <input className="new-todo" autofocus onkeydown={ onEnter }
                autocomplete="off" placeholder="What needs to be done?"
            />
        </header>
    );
}

export const links = [
    {hash: '#/', name: 'All'},
    {hash: '#/active', name: 'Active'},
    {hash: '#/completed', name: 'Completed'}
];

/**
 * Stateless Footer component
 */
export function Foot({left, done, route, onClear}) {
    return (
        <footer className="footer">
                <span className="todo-count">
                    <strong>{ left }</strong> { left > 1 ? 'items' : 'item' } left
                </span>
                <ul className="filters">
                    {
                        links.map(({hash, name}) => (
                            <li>
                                <a href={ hash } className={ name.toLowerCase() === route ? 'selected' : '' }>
                                    { name }
                                </a>
                            </li>
                        ))
                    }
                </ul>
                { done > 0 ? (
                    <button className="clear-completed" onClick={ onClear }>Clear completed</button>
                ) : null }
            </footer>
    );
}
